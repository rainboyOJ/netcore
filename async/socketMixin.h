// 对socket进行封装
#pragma once

#include "basic.hpp"
#include "io_context.h"
#include "protocol.h"

namespace netcore {

    void setnonblocking(NativeSocket fd);
    
    NativeSocket open_socket(Protocol const& protocol, bool blocking = false);

    void bind_socket(NativeSocket socket, Endpoint const& endpoint);

    class SocketMixin {

    protected:
        friend class Acceptor;
        friend class AcceptorImpl;
        friend class AsyncReceiveAwaiter;
        friend class AsyncSendAwaiter;

        IoContext*   m_ctx;
        Protocol     m_protocol;
        Endpoint     m_endpoint;
        NativeSocket m_socket;
        bool m_added_to_event_pool;

    public:

        SocketMixin();
        SocketMixin(IoContext &ctx);
        void reset_io_context(IoContext &ctx, SocketMixin &socket);
        NativeSocket native_handle() const noexcept;
        //创建一个socket
        void open(Protocol const& protocol, bool blocking = false);
        // 重置
        void reset();
    };

} // end namespace netcore
