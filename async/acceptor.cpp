#include "acceptor.h"

namespace netcore {


// AcceptorCallback

    void AcceptorCallback::on_callback(IoEvent &evt) {
        TINYASYNC_GUARD("AcceptorCallback.callback(): ");
        auto acceptor = m_acceptor;
        ListNode* node = acceptor->m_awaiter_que.pop();
        if (node) {
            // it's ready to accept
            auto conn_sock = ::accept(acceptor->m_socket, NULL, NULL);
            if (conn_sock == -1) {
                if(errno == EAGAIN || errno == EWOULDBLOCK) {
                    return;                    
                } else {
                    //real error
                    // wakeup awaiter
                }
            }
            log("acceptor connection",conn_sock);
            AcceptorAwaiter *awaiter = AcceptorAwaiter::from_node(node);
            awaiter->m_conn_socket = conn_sock;
            //TINYASYNC_RESUME(awaiter->m_suspend_coroutine);
            awaiter->m_suspend_coroutine.resume();
        } else {
            // this will happen when after accetor.listen() but not yet co_await acceptor.async_accept()
            // you should have been using level triger to get this event the next time
            TINYASYNC_LOG("No awaiter found, event ignored");
        }
    }


/**===================== AcceptorAwaiter */

    AcceptorAwaiter::AcceptorAwaiter(Acceptor& acceptor)
        :m_acceptor(&acceptor)
    {
    }

    bool AcceptorAwaiter::await_suspend(std::coroutine_handle<TaskPromise> h)
    {
        //TINYASYNC_ASSERT(m_acceptor);
        auto acceptor = m_acceptor;
        acceptor->m_awaiter_que.push(&this->m_node); //把AcceptorAwaiter加入到m_awaiter_que
        m_suspend_coroutine = h;
        //TINYASYNC_GUARD("AcceptorAwaiter::await_suspend(): ");
        if (!m_acceptor->m_added_to_event_pool) { //只加ioCtx入一次
            int listenfd = m_acceptor->m_socket;
            epoll_event evt;
            evt.data.ptr = &m_acceptor->m_callback; //回调函数

            // level triger by default
            // one thread one event
            evt.events = EPOLLIN | EPOLLEXCLUSIVE;
            auto epfd = m_acceptor->m_ctx->event_poll_handle();
            auto ctlerr = epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &evt);
            if (ctlerr == -1) {
                TINYASYNC_LOG("can't await accept %s (epoll %s)", socket_c_str(listenfd), handle_c_str(epfd));
                //throw_errno(format("can't await accept %x (epoll %s)", socket_c_str(listenfd), handle_c_str(epfd)));
            }
            m_acceptor->m_added_to_event_pool = true;
        }
        return true;
    }

    Connection AcceptorAwaiter::await_resume(){
        auto acceptor = m_acceptor;
        //acceptor->m_awaiter_que.pop(); // TODO pop了两次?
        NativeSocket conn_sock = m_conn_socket;

        if(conn_sock == -1) {
            //throw_errno(format("can't accept, socket = %s", socket_c_str(conn_sock)).c_str());
            throw;
        }

        TINYASYNC_LOG("setnonblocking, socket = %s", socket_c_str(conn_sock));
        setnonblocking(conn_sock);
                
        //TINYASYNC_ASSERT(conn_sock != NULL_SOCKET);
        //return { *acceptor->m_ctx, conn_sock, false };
        return {};
    }


/**===================== Acceptor */

    Acceptor::Acceptor(IoContext& ctx, Protocol protocol, Endpoint endpoint)
        :Acceptor(ctx)
    {
        init(&ctx, protocol, endpoint);
    }


    Acceptor::Acceptor(Protocol protocol, Endpoint endpoint)
        :Acceptor()
    {
        init(nullptr,protocol,endpoint);
    }

    void Acceptor::init(IoContext *, Protocol const& protocol, Endpoint const& endpoint)
    {
        try {
            open(protocol); // 创建一个socket 给 m_socket
            bind_socket(m_socket,endpoint); 
            m_endpoint = endpoint;
            listen(); //进入监听
        }
        catch(...){
            reset();
            auto what = format("open/bind/listen failed %s:%d", endpoint.address().to_string().c_str(), (int)(unsigned)endpoint.port());
            std::throw_with_nested(std::runtime_error(what));
        }
    }

    void Acceptor::reset_io_context(IoContext &ctx, Acceptor &r)
    {
        SocketMixin::reset_io_context(ctx, r);
    }

    void Acceptor::listen()
    {
        int max_pendding_connection = 5;// TODO change this
        int err = ::listen(m_socket, max_pendding_connection);
        log("listen at ",m_endpoint.port());
        if (err == -1) {
            throw_errno("can't listen socket");
        }
    }

    AcceptorAwaiter Acceptor::async_accept()
    {
        return {*this};
    }

} // end namespace netcore

