// @desc 创建一个coroutine的resturn Object

#pragma once

#include "basic.hpp"


namespace netcore {


//? 为什么要 wrap exception_ptr
struct ExceptionPtrWrapper
{

    std::exception_ptr &exception() {
        return (std::exception_ptr &)m_exception;//指针转引用
    }

    ExceptionPtrWrapper() {
        new(m_exception) std::exception_ptr();//布局参数
    }        

    ~ExceptionPtrWrapper()
    {
    }
private:
    alignas(std::exception_ptr) char m_exception[sizeof(std::exception_ptr)];
};


struct TaskPromiseBase
{
public:
    // resumer to destruct exception
    ExceptionPtrWrapper m_unhandled_exception;
    std::coroutine_handle<void> m_continuation;

    std::coroutine_handle<TaskPromiseBase> coroutine_handle_base() noexcept
    {
        return std::coroutine_handle<TaskPromiseBase>::from_promise(*this);
    }

    TaskPromiseBase() = default;
    TaskPromiseBase(TaskPromiseBase&& r) = delete;
    TaskPromiseBase(TaskPromiseBase const& r) = delete;

    std::suspend_always initial_suspend()
    {
        return { };
    }

    struct FinalAwaiter : std::suspend_always
    {
        // 返回一个其它 coroutine_handle的coroutine_handle ,那个handle resume
        template<class Promise>
        std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> h) const noexcept
        {
            auto &promise = h.promise();
            auto continuum = promise.m_continuation;
            return continuum;
        }
    };

    FinalAwaiter final_suspend() noexcept //返回一个自己的awaiter
    {
        return {};
    }

    void unhandled_exception()
    {
        m_unhandled_exception.exception() = std::current_exception();
    }
    
};

template<class Result>
class PromiseResultMixin  {
public:
    Result m_result;

    PromiseResultMixin() = default;
    
    template<class T>
    std::suspend_always yield_value(T &&v) {
        m_result = std::forward<T>(v);
        return {};
    }

    template<class T>
    void return_value(T &&value)
    {
        m_result = std::forward<T>(value);
    }

    Result &result()
    {
        return m_result;
    }
    
};

template<> // void 特化
class PromiseResultMixin<void>  {
public:
    void return_void() { }
};


template<class Result >
class TaskPromiseWithResult : 
    public TaskPromiseBase,
    public PromiseResultMixin<Result>
{
};




template<class T>
struct AddRef {
    using type = T &;
};

template<>
struct AddRef<void> {
    using type = void;
};




template<class Result = void>
class TINYASYNC_NODISCARD Task
{
public:
    using promise_type          = TaskPromiseWithResult<Result>; // promise_type
    using coroutine_handle_type = std::coroutine_handle<promise_type>;
    using result_type           = Result;
private:
    coroutine_handle_type m_h;
public:


    coroutine_handle_type coroutine_handle()
    {
        return m_h;
    }

    std::coroutine_handle<TaskPromiseBase> coroutine_handle_base() noexcept
    {
        TaskPromiseBase &promise = m_h.promise();
        return std::coroutine_handle<TaskPromiseBase>::from_promise(promise);
    }

    promise_type& promise()
    {
        return m_h.promise();
    }

    template<class R = Result>
    std::enable_if_t<!std::is_same_v<R,void>, typename AddRef<R>::type>
    result() {
        auto &promise = m_h.promise();
        return promise.m_result;
    }

#ifdef TINYASYNC_TASK_OPO_COAWAIT
    struct TINYASYNC_NODISCARD Awaiter
    {
        std::coroutine_handle<promise_type> m_h;
        Awaiter(std::coroutine_handle<promise_type> h) : m_h(h)
        {
        }
#endif
        bool await_ready() noexcept
        {
            return false;
        }

        template<class Promise>
        std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> awaiting_coro)
        {
            auto sub_coroutine = m_h;
            sub_coroutine.promise().m_continuation = awaiting_coro;
            return sub_coroutine;
        }

        Result await_resume() {
            auto sub_coroutine = m_h;
            TINYASYNC_ASSERT(sub_coroutine.done());

            auto &promise = sub_coroutine.promise();
            
            if(promise.m_unhandled_exception.exception()) {
                reset_and_throw_coro(sub_coroutine);
                //reset_and_throw_exception(promise.m_unhandled_exception.exception());
            }

            if(!sub_coroutine) {
                TINYASYNC_UNREACHABLE();
            }

            if constexpr (!std::is_same_v<void, Result>) {
                return std::move(sub_coroutine.promise().m_result);
            }

        }

#ifdef TINYASYNC_TASK_OPO_COAWAIT
    };
    Awaiter operator co_await()
    {
        return { m_h };
    }
#endif
    struct TINYASYNC_NODISCARD JoinAwaiter
    {            
        std::coroutine_handle<promise_type> m_sub_coroutine;

        JoinAwaiter(std::coroutine_handle<promise_type> h) : m_sub_coroutine(h)
        {
        }

        bool await_ready() noexcept
        {
            return m_sub_coroutine.done();
        }

        template<class Promise>
        void await_suspend(std::coroutine_handle<Promise> awaiting_coro)
        {
            auto sub_coroutine = m_sub_coroutine;
            sub_coroutine.promise().m_continuation = awaiting_coro;
        }

        Result await_resume();

    };

    // Resume coroutine
    // This is a help function for:
    // resume_coroutine(task.coroutine_handle())
    //
    // Return false, if coroutine is done. otherwise, return true
    // The coroutine is destroyed, if it is done and detached (inside of the coroutine)
    // E.g:
    // Task foo(Task **task) {  (*task)->detach(); }
    // Task *ptask;
    // Task task(&ptask);
    // ptask = &task;
    // task.resume();
    // the coroutine will is destoryed
    bool resume();

    JoinAwaiter join()
    {
        return { m_h };
    }


    Task() : m_h(nullptr)
    {
    }

    Task(coroutine_handle_type h) : m_h(h)
    {
    }

    Task(Task&& r) noexcept : m_h(std::exchange(r.m_h, nullptr))
    {
    }

    Task& operator=(Task&& r) noexcept
    {
        this->~Task();
        m_h = r.m_h;
        r.m_h = nullptr;
        return *this;
    }

    void swap(Task &r) {
        std::swap(this->m_h, r.m_h);
    }

    ~Task()
    {
        if (m_h) {
            // unset_name();
            {
                TINYASYNC_GUARD("coroutine_handle.destroy(): ");
                TINYASYNC_LOG("");
                m_h.destroy();
            }
        }
    }

    // Release the ownership of cocoutine.
    // Now you are responsible for destroying the coroutine.
    std::coroutine_handle<promise_type> release()
    {
        auto h = m_h;
        m_h = nullptr;
        return h;
    }

    // Release the ownership of coroutine.
    // mark the coroutine it is detached.
    std::coroutine_handle<promise_type> detach()
    {
        promise().m_dangling = true;
        return release();
    }


    Task(Task const& r) = delete;
    Task& operator=(Task const& r) = delete;
};



inline void resume_coroutine_task(std::coroutine_handle<TaskPromiseBase> coroutine)
{
    coroutine.resume();
    if(coroutine.promise().m_unhandled_exception.exception()) TINYASYNC_UNLIKELY {
        reset_and_throw_exception(coroutine.promise().m_unhandled_exception.exception());
    }
}

inline void resume_coroutine_callback(std::coroutine_handle<TaskPromiseBase> coroutine)
{
    coroutine.resume();
    // the last coroutine is not always the same as the resume coroutine
}


} // end namespace netcore


