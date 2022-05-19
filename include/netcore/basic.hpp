//@desc 基础

#pragma once


#if defined(__clang__)

#include <experimental/coroutine>
#include <experimental/memory_resource>
namespace std {
    using std::experimental::suspend_always;
    using std::experimental::suspend_never;
    using std::experimental::coroutine_handle;
    using std::experimental::noop_coroutine;
    using std::experimental::coroutine_traits;
    namespace pmr {
        using std::experimental::pmr::memory_resource;
        using std::experimental::pmr::get_default_resource;
        using std::experimental::pmr::set_default_resource;
    }
}
#else

#include <coroutine>
#include <memory_resource>

#endif

#include <atomic>
#include <cstring>
#include <exception>
#include <utility>
#include <map>
#include <unordered_map>
#include <string>
#include <vector>
#include <type_traits>
#include <system_error>

#include <iostream>
#include <stdarg.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <memory.h>
#include <memory>
#include <list>
#include <mutex>
#include <atomic>
#include <chrono>
#include <functional>
#include <new>
#include <mutex>


#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <cxxabi.h>
#include <sys/eventfd.h>
#include <netinet/tcp.h>
#include <pthread.h>


#include "net_error.hpp"

#define TINYASYNC_NODISCARD
#define TINYASYNC_UNLIKELY

#define TINYASYNC_UNREACHABLE(...)
#define TINYASYNC_GUARD(...)
#define TINYASYNC_LOG(...)


#define throw_errno throw

namespace netcore {


    static std::mutex mtx;
    template<typename... T>
    void log(T... args){
        std::lock_guard<std::mutex> lck(mtx);
        ( (std::cout << args << " "),...);
        std::cout << '\n';
    }

    inline std::string vformat(char const* fmt, va_list args)
    {
        va_list args2;
        va_copy(args2, args);
        std::size_t n = vsnprintf(NULL, 0, fmt, args2);
        std::string ret;
        ret.resize(n);
        vsnprintf((char*)ret.data(), ret.size() + 1, fmt, args);
        return ret;
    }

    inline std::string format(char const* fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        std::string str = vformat(fmt, args);
        va_end(args);
        return str;
    }

    using NativeHandle = int;
    using IoEvent = epoll_event;
    using NativeSocket = int;
    constexpr NativeSocket const NULL_SOCKET = NativeSocket(0);


    constexpr const int NULL_HANDLE = 0;

    struct offset_of_impl {
        template<typename T, typename U> 
        static constexpr size_t offsetOf(U T::*member)
        {
            return (char*)&((T*)nullptr->*member) - (char*)nullptr;
        }
    };

    struct ListNode {
        ListNode *m_next = nullptr;
    };

    struct Queue
    {
        ListNode m_before_head;
        ListNode *m_tail = nullptr;

#ifndef TINYASYNC_NDEBUG
        std::atomic<int> queue_size = 0;
#endif

        Queue() = default;
        
        Queue(Queue const &r) {
            m_before_head = r.m_before_head;
            m_tail = r.m_tail;
#ifndef TINYASYNC_NDEBUG
            queue_size.store(r.queue_size.load());
#endif
        }

        Queue operator=(Queue const &r) {
            m_before_head = r.m_before_head;
            m_tail = r.m_tail;
#ifndef TINYASYNC_NDEBUG
            queue_size.store(r.queue_size.load());
#endif
            return *this;
        }

        std::size_t count()
        {
            std::size_t n = 0;
            for (auto h = m_before_head.m_next; h; h = h->m_next)
            {
                n += 1;
            }
            return n;
        }

        void clear() {
            m_before_head.m_next = nullptr;
            m_tail = nullptr;
        }

        // consume a dangling ndoe
        void push(ListNode *node)
        {
            //TINYASYNC_ASSERT(node);
            node->m_next = nullptr;
            auto tail = this->m_tail;
            if (tail == nullptr)
            {
                tail = &m_before_head;
            }
            tail->m_next = node;
            this->m_tail = node;

#ifndef TINYASYNC_NDEBUG
            ++queue_size;
#endif
        }

        // return a dangling node
        // node->m_next is not meaningful
        ListNode *pop(bool &is_empty)
        {
            auto *before_head = &this->m_before_head;
            auto head = before_head->m_next;
            if(head) {
                auto new_head = head->m_next;
                before_head->m_next = new_head;
                bool is_empty_ = new_head == nullptr;
                if (is_empty_)
                {
    #ifndef TINYASYNC_NDEBUG
                    //TINYASYNC_ASSERT(queue_size == 1);
    #endif
                    m_tail = nullptr;
                }

    #ifndef TINYASYNC_NDEBUG
                --queue_size;
    #endif
    
            }
            return head;
        }

        // return a dangling node
        // node->m_next is not meaningful
        ListNode *pop()
        {
            bool empty__;
            return this->pop(empty__);
        }

        // return a dangling node
        // node->m_next is not meaningful
        ListNode *pop_nocheck(bool &is_empty)
        {
            auto *before_head = &this->m_before_head;
            auto head = before_head->m_next;
            //TINYASYNC_ASSERT(head);
            if(true) {
                auto new_head = head->m_next;
                before_head->m_next = new_head;
                bool is_empty_ = new_head == nullptr;
                if (is_empty_)
                {
    #ifndef TINYASYNC_NDEBUG
                    //TINYASYNC_ASSERT(queue_size == 1);
    #endif
                    m_tail = nullptr;
                }

    #ifndef TINYASYNC_NDEBUG
                --queue_size;
    #endif
    
                is_empty = is_empty_;
            }
            return head;
        }
    };


    inline char const* handle_c_str(NativeHandle handle)
    {
        static std::map<NativeHandle, std::string> handle_map;
        auto& str = handle_map[handle];
        if (str.empty()) {
            str = format("%d", handle);
        }
        return str.c_str();
        
    }

    struct Callback
    {
        // we don't use virtual table for two reasons
        //     1. virtual function let Callback to be non-standard_layout, though we have solution without offsetof using inherit
        //     2. we have only one function ptr, thus ... we can save a memory load without virtual functions table
        using CallbackPtr = void (*)(Callback *self, IoEvent &);

        CallbackPtr m_callback;

        void callback(IoEvent &evt)
        {
            this->m_callback(this, evt);
        }

    };

    struct CallbackImplBase : Callback
    {

        // implicit null is not allowed
        CallbackImplBase(std::nullptr_t)
        {
            m_callback = nullptr;
        }

        template <class SubclassCallback>
        CallbackImplBase(SubclassCallback *)
        {
            (void)static_cast<SubclassCallback *>(this);
            //memset(&m_overlapped, 0, sizeof(m_overlapped));
            m_callback = &invoke_impl_callback<SubclassCallback>;
        }

        template <class SubclassCallback>
        static void invoke_impl_callback(Callback *this_, IoEvent &evt)
        {
            // invoke subclass' on_callback method
            SubclassCallback *subclass_this = static_cast<SubclassCallback *>(this_);
            subclass_this->on_callback(evt); //调用子类的on_callback
        }
    };
    static constexpr std::size_t CallbackImplBase_size = sizeof(CallbackImplBase);
    static_assert(std::is_standard_layout_v<CallbackImplBase>);


    inline int close_socket(NativeSocket socket)
    {
        return ::close(socket);
    }

} // end namespace netcore
