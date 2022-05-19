// 接收连接
#pragma once

#include "basic.hpp"
#include "io_context.h"
#include "task.h"
#include "protocol.h"
//#include "socketMixin.h"
#include "connection.h"

namespace netcore {

    void setnonblocking(NativeSocket fd);
    void bind_socket(NativeSocket socket, Endpoint const& endpoint);
    NativeSocket open_socket(Protocol const& protocol, bool blocking = false);

    struct Address;
    struct Endpoint;
    struct Protocol;

    struct Acceptor;

    class AcceptorCallback : public CallbackImplBase { friend class Acceptor;
        Acceptor* m_acceptor;
    public:
        AcceptorCallback(Acceptor* acceptor) 
            : CallbackImplBase(this), m_acceptor { acceptor }
        {}

        void on_callback(IoEvent& evt);
    };
    
    struct AcceptorAwaiter {
        
        friend class Acceptor;
        friend class AcceptorCallback;

        ListNode  m_node;
        Acceptor* m_acceptor;
        NativeSocket m_conn_socket;
        std::coroutine_handle<TaskPromise> m_suspend_coroutine;

    public:

        static AcceptorAwaiter *from_node(ListNode *node) {
            return (AcceptorAwaiter *)((char*)node - offsetof(AcceptorAwaiter, m_node));
        }

        bool await_ready() { return false; }
        AcceptorAwaiter(Acceptor& acceptor);

        template<class Promise>
        inline void await_suspend(std::coroutine_handle<Promise> suspend_coroutine) {
            auto h = suspend_coroutine.promise().coroutine_handle();
            await_suspend(h);
        }
        /**
         * 将 Acceptor的m_socket加入到epoll里监听,设置回调用函数
         */
        bool await_suspend(std::coroutine_handle<TaskPromise> h);
        Connection::CONN_PTR await_resume();
    };

    struct Acceptor {
        private:
            friend class AcceptorAwaiter;
            friend class AcceptorCallback;
            
            IoContext * m_ctx;
            Protocol     m_protocol;
            Endpoint     m_endpoint;
            NativeSocket m_socket;
            bool m_added_to_event_pool;

            AcceptorCallback m_callback = this;
            Queue m_awaiter_que; //存awaiter的queue
        public:
            Acceptor();
            Acceptor(IoContext & ctx);

            Acceptor(IoContext& ctx, Protocol protocol, Endpoint endpoint);
            Acceptor(Protocol protocol, Endpoint endpoint);

            //初始化构造
            void init(IoContext *, Protocol const& protocol, Endpoint const& endpoint);
            //重置
            void reset_io_context(IoContext &ctx, Acceptor &r);
            //监听
            void listen();
            // 异步的接收连接
            AcceptorAwaiter async_accept();
            //同步的连接,不需要注册到ioConentx
            NativeSocket accept();

            void reset();

        private:
    };

} // end namespace netcore


