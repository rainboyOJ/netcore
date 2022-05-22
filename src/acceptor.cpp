#include "acceptor.h"

namespace netcore {

    void setnonblocking(NativeSocket fd)
    {
        //int status = fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
        int status = fcntl(fd, F_SETFL, O_NONBLOCK);

        if (status == -1) {
            throw std::system_error(errno, std::system_category(), "can't set fd nonblocking");
        }
    }


    NativeSocket open_socket(Protocol const& protocol, bool blocking)
    {
//#ifdef TINYASYNC_THROW_ON_OPEN
        //throw_error("throw on open", 0);
//#endif
        TINYASYNC_GUARD("open_socket(): ");


        // PF means protocol
        // man 2 socket 查看手册
        // AF_INET 使用IPV4 版本,本程序暂时还没有支持IPV6
        auto socket_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (socket_ == -1) {
            throw std::runtime_error("can't create socket");
        }
        if (!blocking)
            setnonblocking(socket_);
        TINYASYNC_LOG("create socket %s, nonblocking = %d", socket_c_str(socket_), int(!blocking));
        return socket_;
    }

    void bind_socket(NativeSocket socket, Endpoint const& endpoint)
    {
        TINYASYNC_GUARD("bind_socket(): ");
        TINYASYNC_LOG("socket = %s", socket_c_str(socket));

        int binderr;
        if(endpoint.address().m_address_type == AddressType::IpV4)
        {
            sockaddr_in serveraddr;
            memset(&serveraddr, 0, sizeof(serveraddr));
            serveraddr.sin_family = AF_INET;
            serveraddr.sin_port = htons(endpoint.port());
            serveraddr.sin_addr.s_addr = endpoint.address().m_addr4.s_addr;
            binderr = ::bind(socket, (sockaddr*)&serveraddr, sizeof(serveraddr));

        } else if(endpoint.address().m_address_type == AddressType::IpV6) {
            sockaddr_in6 serveraddr;
            memset(&serveraddr, 0, sizeof(serveraddr));
            serveraddr.sin6_family = AF_INET6;
            serveraddr.sin6_port = htons(endpoint.port());
            serveraddr.sin6_addr = endpoint.address().m_addr6;
            binderr = ::bind(socket, (sockaddr*)&serveraddr, sizeof(serveraddr));
        }

        if (binderr == -1) {
            //throw_errno(format("can't bind socket, fd = %x", socket));
            throw std::runtime_error(format("can't bind socket, fd = %x", socket));
        }
    }


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
                //log("can't await accept %s (epoll %s)", socket_c_str(listenfd), handle_c_str(epfd));
                log("=====================add epoll ctl error,epoll_fd",std::to_string(epfd),"listenfd",std::to_string(listenfd));
                //throw_errno(format("can't await accept %x (epoll %s)", socket_c_str(listenfd), handle_c_str(epfd)));
                throw std::runtime_error("add epoll ctl error");
            }
            log("listenfd",listenfd,"add epoll",std::to_string(epfd),"succ");
            m_acceptor->m_added_to_event_pool = true;
        }
        return true;
    }

    Connection::CONN_PTR AcceptorAwaiter::await_resume(){
        auto acceptor = m_acceptor;
        //acceptor->m_awaiter_que.pop(); // TODO pop了两次?
        NativeSocket conn_sock = m_conn_socket;

        if(conn_sock == -1) {
            //throw_errno(format("can't accept, socket = %s", socket_c_str(conn_sock)).c_str());
            throw std::runtime_error("can't accept");
        }

        TINYASYNC_LOG("setnonblocking, socket = %s", socket_c_str(conn_sock));
        setnonblocking(conn_sock);
                
        //TINYASYNC_ASSERT(conn_sock != NULL_SOCKET);
        //return { *acceptor->m_ctx, conn_sock, false };
        //return { acceptor->m_ctx, conn_sock};
        return  std::make_unique<Connection>(acceptor->m_ctx,conn_sock);
    }


/**===================== Acceptor */

    Acceptor::Acceptor(){
        m_ctx = nullptr;
        m_socket = NULL_SOCKET;
        m_added_to_event_pool = false;
    }

    Acceptor::Acceptor(IoContext& ctx){
        m_ctx = &ctx;
        m_socket = NULL_SOCKET;
        m_added_to_event_pool = false;
    }

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
            //open(protocol); // 创建一个socket 给 m_socket

            m_socket = open_socket(protocol);
            //
            //
            struct linger optLinger = { 0 };
            /* 优雅关闭: 直到所剩数据发送完毕或超时 */
            optLinger.l_onoff = 1;
            optLinger.l_linger = 1;

            int ret = setsockopt(m_socket, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
            if(ret == -1) {
                //LOG(ERROR) << ("set socket setsockopt error !");
                throw std::runtime_error("set socket setsockopt error !");
                //close(listenFd_);
                //return false;
            }

            int optval = 1;
            ret = setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
            if(ret == -1) {
                throw std::runtime_error("set socket setsockopt error !");
                //LOG(ERROR) << ("set socket setsockopt error !");
                //close(listenFd_);
                //return false;
            }

            m_protocol = protocol;

            bind_socket(m_socket,endpoint); 
            m_endpoint = endpoint;
            listen(); //进入监听
            log("listenfd is ",m_socket);
        }
        catch(...){
            reset();
            auto what = format("open/bind/listen failed %s:%d", endpoint.address().to_string().c_str(), (int)(unsigned)endpoint.port());
            //std::throw_with_nested(std::runtime_error(what));
            throw std::runtime_error(what);
        }
    }

    void Acceptor::reset()
    {
        TINYASYNC_GUARD("SocketMixin.reset(): ");
        if (m_socket) {

            if (m_ctx && m_added_to_event_pool) {
                // this socket may be added to many pool
                // ::close can't atomatically remove it from event pool
                auto ctlerr = epoll_ctl(m_ctx->event_poll_handle(), EPOLL_CTL_DEL, m_socket, NULL);
                if (ctlerr == -1) {
                    auto what = format("can't remove (from epoll) socket %x", m_socket);
                    throw std::runtime_error(what);
                }
                m_added_to_event_pool = false;
            }
            TINYASYNC_LOG("close socket = %s", socket_c_str(m_socket));
            if(close_socket(m_socket) < 0) {
                printf("%d\n", errno);
                std::exit(1);
            }
            m_socket = NULL_SOCKET;
        }
    }

    void Acceptor::reset_io_context(IoContext &ctx, Acceptor &r)
    {
        m_ctx = &ctx;
        m_protocol = r.m_protocol;
        m_endpoint = r.m_endpoint;
        m_socket = r.m_socket;
        m_added_to_event_pool = false;
    }

    void Acceptor::listen()
    {
        int max_pendding_connection = 5;// TODO change this
        int err = ::listen(m_socket, max_pendding_connection);
        log("listen at ",m_endpoint.port());
        if (err == -1) {
            throw std::runtime_error("can't listen socket");
        }
    }

    AcceptorAwaiter Acceptor::async_accept()
    {
        return {*this};
    }

} // end namespace netcore

