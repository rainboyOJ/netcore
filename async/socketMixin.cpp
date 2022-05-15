#include "socketMixin.h"

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
#ifdef TINYASYNC_THROW_ON_OPEN
        throw_error("throw on open", 0);
#endif
        TINYASYNC_GUARD("open_socket(): ");


        // PF means protocol
        // man 2 socket 查看手册
        // AF_INET 使用IPV4 版本,本程序暂时还没有支持IPV6
        auto socket_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (socket_ == -1) {
            throw("can't create socket");
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

    SocketMixin::SocketMixin()
    {
        m_ctx = nullptr;
        m_socket = NULL_SOCKET;
        m_added_to_event_pool = false;
    }

    SocketMixin::SocketMixin(IoContext &ctx)
    {
        m_ctx = &ctx;
        m_socket = NULL_SOCKET;
        m_added_to_event_pool = false;
    }

    void SocketMixin::reset_io_context(IoContext &ctx, SocketMixin &socket)
    {
        m_ctx = &ctx;
        m_protocol = socket.m_protocol;
        m_endpoint = socket.m_endpoint;
        m_socket = socket.m_socket;
        m_added_to_event_pool = false;
    }

    NativeSocket SocketMixin::native_handle() const noexcept
    {
        return m_socket;
    }


    void SocketMixin::open(Protocol const& protocol, bool blocking)
    {
        m_socket = open_socket(protocol, blocking);
        m_protocol = protocol;
    }

    void SocketMixin::reset()
    {
        TINYASYNC_GUARD("SocketMixin.reset(): ");
        if (m_socket) {

            if (m_ctx && m_added_to_event_pool) {
                // this socket may be added to many pool
                // ::close can't atomatically remove it from event pool
                auto ctlerr = epoll_ctl(m_ctx->event_poll_handle(), EPOLL_CTL_DEL, m_socket, NULL);
                if (ctlerr == -1) {
                    auto what = format("can't remove (from epoll) socket %x", m_socket);
                    throw(what);
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
} // end namespace netcore

