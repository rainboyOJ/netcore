// 新的task ,没有原来那么复杂,只能有void值,也就是不接收值
#pragma once

#include "basic.hpp"

namespace netcore {

struct Task;


struct TaskPromise {
    //std::coroutine_handle<void> m_continuation;

    std::coroutine_handle<TaskPromise> m_h;
    std::exception_ptr m_except{nullptr};

    std::coroutine_handle<TaskPromise>
    coroutine_handle() const
    {
        return m_h;
    }

    TaskPromise() = default;
    TaskPromise(TaskPromise const & ) = delete;
    TaskPromise(TaskPromise && ) = delete;

    Task get_return_object();
    std::suspend_always initial_suspend()
    {
        return {};
    }

    //std::suspend_always final_suspend() noexcept
    //{
        //return {};
    //}

    std::suspend_never final_suspend() noexcept
    {
        //if( m_except !=nullptr){
            //std::rethrow_exception(m_except);
        //}
        return {};
    }
    void unhandled_exception() // TODO 如何处理异常呢?
    {
        //m_unhandled_exception.exception() = std::current_exception();
        log(">>>>>>>>unhandled_exception");
        m_except = std::current_exception();
        std::rethrow_exception(m_except);
    }
    void return_void() {
        //if( m_except !=nullptr) {
            //std::rethrow_exception(m_except);
        //}
    };
};

struct Task {
    public:
        using promise_type = TaskPromise;
        using coroutine_handle_type =  std::coroutine_handle<promise_type>;
    private:
        coroutine_handle_type m_h;
    public:
        Task(coroutine_handle_type h);
        coroutine_handle_type coroutine_handle()
        {
            return m_h;
        }

        promise_type& promise()
        {
            return m_h.promise();
        }

        //awaitable
        bool await_ready() noexcept
        {
            return false;
        }

        template<typename Promise>
        coroutine_handle_type await_suspend(std::coroutine_handle<Promise> awaiting_coro)
        {
            return m_h;
        }
        void await_resume() {
            log("task await_resume");
            if(m_h.promise().m_except !=nullptr ){
                log("task await_resume catch error");
                std::rethrow_exception(m_h.promise().m_except);
            }
        }
};



//============ co_spawn
struct SpawnTask;
struct SpawnTaskPromise {
    std::suspend_never initial_suspend() { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }

    void unhandled_exception() {
        //TINYASYNC_RETHROW();
    }

    SpawnTask get_return_object();
    void return_void() { }
};

struct SpawnTask
{
    using promise_type = SpawnTaskPromise;
    SpawnTask(std::coroutine_handle<> h) : m_handle(h)
    {}
    std::coroutine_handle<> m_handle;
};

//template<class Result>
inline SpawnTask co_spawn(Task task)
{
    co_await task;
    // help simpilfy the destructor of task
    //if(!task.coroutine_handle()) {
        //TINYASYNC_UNREACHABLE();
    //}
}

} // end namespace netcore

