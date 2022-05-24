#include "connection.h"

namespace netcore {

    long long recv(NativeSocket sock,char *buf,std::size_t buff_size){
        log("recv....");
        std::size_t recv_size = 0;
        for( ;;) {
            int nbytes = ::recv(sock, buf+recv_size, buff_size-recv_size,0 );
            if( nbytes < 0){
                if( (errno == EAGAIN || errno == EWOULDBLOCK ) ) 
                    //读取成功
                    return recv_size;
                else {
                    //throw std::runtime_error("errno may be disconnection");
                    throw netcore::RecvError(std::strerror(errno));
                }
            }
            else if( nbytes == 0) { //对就关闭连接
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
            nbytes = ::send(sock, buf+sent, send_size, MSG_NOSIGNAL);
            if( nbytes < 0 ) {
                if( (errno == EAGAIN || errno == EWOULDBLOCK ) ) 
                {
                    return sent;
                }
                else {
                    throw netcore::SendError(std::strerror(errno));
                }
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

        if( ( events & EPOLLIN ) && m_connection !=nullptr )
        {
            log("EVENt run RECV >>>>");
            m_connection ->m_recv_at_awaiter = false;
            //读取一直读取完毕
            auto awaiter = recv_awaiter;
            try {
                awaiter->m_tranfer_bytes = netcore::recv(m_connection->m_socket, awaiter->m_buff, awaiter->m_buff_size);
            }
            catch(...) {
                m_connection->m_except = std::current_exception();
            }
            awaiter->m_h.resume();
        }

        if( (events & EPOLLOUT ) && m_connection->m_send_awaiter != nullptr)
        {
            log("EVENt run send >>>>");
            m_connection->m_send_at_awaiter = false;
            try {
                long long nbytes  = netcore::send(m_connection->m_socket, send_awaiter->buf(), send_awaiter->left_buf_size());
                send_awaiter->update_sent_size(nbytes);
            }
            catch(...) {
                log("connection send error");
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
    AsyncIoWaiterBases::AsyncIoWaiterBases(Connection * conn,char * buff,std::size_t buff_size,std::chrono::seconds time_) 
        : m_conn{conn},m_buff{buff},m_buff_size{buff_size}
    {
        m_end_time = std::chrono::system_clock::now() + time_;
        //创建一个全局的PostTask
        m_post_ptr = new PostTask;
        m_post_ptr->set_callback(check_deley);
        m_post_ptr->set_ref_count(1);
        m_post_ptr->set_awaiter_ptr(&m_node);
        //加入队列中
        m_conn->m_ctx->post_task(m_post_ptr);
    }

    AsyncIoWaiterBases::~AsyncIoWaiterBases() {
        m_post_ptr->set_ref_count(0); //表示运行结束
    }

    void AsyncIoWaiterBases::check_deley(PostTask *task_ptr)
    {
        // ref_count 是否为0
        if(task_ptr->invalid()) {
            delete task_ptr; //删除自己
        }
        else { //check time
            //从PostTask 找到awaiter
            auto await_ptr = from_node(task_ptr->get_m_awater_ptr());
            //是否超时
            if( await_ptr->expired() ) {
                //throw 
                await_ptr->m_conn->m_except = std::make_exception_ptr(netcore::IoTimeOut("connection AsyncIoWaiterBases TimeOut")); //创建exception_ptr
                //执行唤醒
                await_ptr->m_h.resume();
            }
            else {
                //把task再次加入队列
                await_ptr->m_conn->m_ctx->post_task(task_ptr);
                log("add",task_ptr,"to ctx queue");
            }
        }
    }

    bool AsyncIoWaiterBases::expired() {
        return  std::chrono::system_clock::now() > m_end_time;
    }

    AsyncReadAwaiter::AsyncReadAwaiter(Connection * conn,char * buff,std::size_t buff_size,std::chrono::seconds time_) 
            :AsyncIoWaiterBases(conn,buff,buff_size,time_)
        {
            //加入任务
            //m_conn->m_ctx->post_task(PostTask *callback)
        }

    bool AsyncReadAwaiter::await_suspend(std::coroutine_handle<TaskPromise> h)
    {
        m_h = h;
        m_conn->m_read_awaiter = this;
        //if(m_conn->m_recv_at_awaiter) {
            //try {
                //this->m_tranfer_bytes = netcore::recv(m_conn->m_socket, this->m_buff, this->m_buff_size);
            //}
            //catch(...){
                //m_conn->m_except = std::current_exception();
            //}
            //return false; //不会挂起
        //}
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
            log("AsyncSendAwaiter await_suspend send");
            try {
                long long nbytes  = netcore::send(m_conn->m_socket, this->buf(), this->left_buf_size());
                this->update_sent_size(nbytes);
            }
            catch(...){
                m_conn->m_except = std::current_exception();
                log("catch error set m_conn m_except");
                return false;
            }
            log("send finshed");
            if(this->finised()) { //发送完毕
                log("send finshed 2");
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
        //evt.data.fd = m_socket;
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

    AsyncReadAwaiter Connection::async_read(char *buf, std::size_t buff_size,std::chrono::seconds time_)
    {
        return {this,buf,buff_size,time_};
    }

    AsyncSendAwaiter Connection::async_send(char *buf, std::size_t buff_size,std::chrono::seconds time_)
    {
        return {this,buf,buff_size,time_};
    }

    Connection::~Connection() {
        log("Connection deconsructor");
        close();
        if( m_except != nullptr) {
            std::rethrow_exception(m_except);
        }
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

    void Connection::clear_except()
    {
        this->m_except = nullptr;
    }



} // end namespace netcore

