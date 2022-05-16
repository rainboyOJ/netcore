#include "connection.h"

namespace netcore {

    long long recv(NativeSocket sock,char *buf,std::size_t buff_size){
        log("recv....");
        std::size_t recv_size = 0;
        for( ;;) {
            int nbytes = ::recv(sock, buf+recv_size, buff_size-recv_size,0 );
            log("nbytes",nbytes);
            if( nbytes < 0){
                if( (errno == EAGAIN || errno == EWOULDBLOCK ) ) 
                    //读取成功
                    return recv_size;
                else {
                    log("recv errno");
                    //throw std::runtime_error("errno may be disconnection");
                    throw netcore::RecvError(std::strerror(errno));
                }
            }
            else if( nbytes == 0) { //对就关闭连接
                log("connection disconnection");
                return 0;
            }
            recv_size += nbytes;
            if( recv_size >= buff_size) //没有空间了
                throw netcore::RecvError("buffer not enough space");
                //throw std::runtime_error("buf not enough space");
        }
    }

    long long send(NativeHandle sock,char * buf,long long send_size){
        int nbytes = 0;
        long long sent = 0;
        while( send_size  > 0) {
            nbytes = ::send(sock, buf+sent, send_size, 0);
            if( nbytes < 0 ) {
                if( (errno == EAGAIN || errno == EWOULDBLOCK ) ) 
                {
                    return sent;
                }
                else 
                    throw netcore::SendError(std::strerror(errno));
                    //throw "send error";
            }
            send_size -= nbytes;
            sent += nbytes;
            //buf += nbytes;
        }
        return sent;
    }


// callback

    void ConnCallBack::on_callback(IoEvent &evt)
    {
        int events = evt.events;

        if(events & EPOLLIN)
            m_connection ->m_recv_at_awaiter = true;
        if( events & EPOLLOUT )
            m_connection ->m_send_at_awaiter = true;

        auto recv_awaiter = m_connection->m_read_awaiter;
        auto send_awaiter = m_connection->m_send_awaiter;

        if( ( events & EPOLLIN ) && recv_awaiter !=nullptr )
        {
            m_connection ->m_recv_at_awaiter = false;
            //读取一直读取完绊
            auto awaiter = recv_awaiter;
            try {
                awaiter->m_tranfer_bytes = netcore::recv(m_connection->m_socket, awaiter->m_buff, awaiter->m_buff_size);
            }
            catch(...) {
                m_connection->m_except = std::current_exception();
            }
            awaiter->m_h.resume();
        }

        if( (events & EPOLLOUT ) && send_awaiter != nullptr)
        {
            m_connection->m_send_at_awaiter = false;
            try {
                long long nbytes  = netcore::send(m_connection->m_socket, send_awaiter->buf(), send_awaiter->left_buf_size());
                send_awaiter->update_sent_size(nbytes);
            }
            catch(...) {
                m_connection->m_except = std::current_exception();
            }

            if(send_awaiter->finised()) { //发送完毕
                send_awaiter->m_h.resume();
            }
            else {
                // try again latter......
            }
        }

        /*
        if (events & ( EPOLLERR | EPOLLHUP) )  {
            // 关闭连接
            // 换起所有的awaiter
            m_connection->close();
        }
        */
        
        //if( (events & (EPOLLOUT | EP)))
    }

// Awaiter

    bool AsyncReadAwaiter::await_suspend(std::coroutine_handle<TaskPromise> h)
    {
        m_h = h;
        m_conn->m_read_awaiter = this;
        if(m_conn->m_recv_at_awaiter) {
            try {
                this->m_tranfer_bytes = netcore::recv(m_conn->m_socket, this->m_buff, this->m_buff_size);
            }
            catch(...){
                m_conn->m_except = std::current_exception();
            }
            return false; //不会挂起
        }
        return true;
    }

    std::size_t AsyncReadAwaiter::await_resume()
    {
        m_conn->m_read_awaiter = nullptr;
        if( m_conn->m_except != nullptr) {
            std::rethrow_exception(m_conn->m_except);
        }
        return m_tranfer_bytes;
    }
    // === AsyncSendAwaiter
    char * AsyncSendAwaiter::buf() {
        return m_buff + m_tranfer_bytes;
    }

    std::size_t AsyncSendAwaiter::left_buf_size() {
        return m_buff_size - m_tranfer_bytes;
    }

    void AsyncSendAwaiter::update_sent_size(std::size_t siz) {
        m_tranfer_bytes+= siz;
    }
    bool AsyncSendAwaiter::finised(){
        return m_tranfer_bytes>= m_buff_size;
    }

    bool AsyncSendAwaiter::await_suspend(std::coroutine_handle<TaskPromise> h) 
    {
        m_h = h;
        m_conn->m_send_awaiter = this;
        if( m_conn -> m_send_at_awaiter ) {
            try {
                long long nbytes  = netcore::send(m_conn->m_socket, this->buf(), this->left_buf_size());
                this->update_sent_size(nbytes);
            }
            catch(...){
                m_conn->m_except = std::current_exception();
            }
            if(this->finised()) { //发送完毕
                return false;
            }
        }
        return true;
    }

    std::size_t AsyncSendAwaiter::await_resume()
    {
        m_conn->m_send_awaiter = nullptr;
        if( m_conn->m_except != nullptr) {
            std::rethrow_exception(m_conn->m_except);
        }
        return m_tranfer_bytes;
    }



//========= connection
    void Connection::registerToIoCtx() {
        IoEvent evt;
        //evt.events = EPOLLONESHOT;
        //evt.events = EPOLLIN | EPOLLOUT;
        evt.events = EPOLLIN | EPOLLOUT | EPOLLEXCLUSIVE | EPOLLET;
        evt.data.fd = m_socket;
        evt.data.ptr = &m_callback;

        int _add_poll_ctl;
        if( m_added_to_event_pool == false){
            _add_poll_ctl = EPOLL_CTL_ADD;
            m_added_to_event_pool = true;
        }
        else {
            _add_poll_ctl = EPOLL_CTL_MOD;
        }
        int ctlerr = epoll_ctl(m_ctx->event_poll_handle(), _add_poll_ctl, m_socket, &evt);
        //TODO Error
        log("add socket to epoll , socket",m_socket);

    }

    void Connection::unRegisterToIoCtx() {
    }

    AsyncReadAwaiter Connection::async_read(char *buf, std::size_t buff_size)
    {
        return {this,buf,buff_size};
    }

    AsyncSendAwaiter Connection::async_send(char *buf, std::size_t buff_size)
    {
        return {this,buf,buff_size};
    }

    Connection::~Connection() {
        log("Connection deconsructor");
        close();
    }

    void Connection::close() {
        log("connection close");
        this->m_recv_at_awaiter = false;
        this->m_send_at_awaiter = false;

        if(this->m_read_awaiter != nullptr){
            this->m_read_awaiter->m_tranfer_bytes = 0;
            this->m_read_awaiter->m_h.resume();
        }

        if(this->m_send_awaiter != nullptr){
            this->m_send_awaiter->m_tranfer_bytes = 0;
            this->m_send_awaiter->m_h.resume();
        }

        if( m_socket !=-1)
            ::close(m_socket);
    }



} // end namespace netcore

