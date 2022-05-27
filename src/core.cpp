#include "core.h"

namespace netcore {

//========== TaskPromise

    Task TaskPromise::get_return_object() {
        m_h = std::coroutine_handle<TaskPromise>::from_promise(*this);
        return { m_h };
    }

//========== Task
    Task::Task(coroutine_handle_type h)
        :m_h{h}
    {}

    Task::coroutine_handle_type Task::coroutine_handle()
    {
        return m_h;
    }

    Task::promise_type& Task::promise()
    {
        return m_h.promise();
    }

    bool Task::await_ready() noexcept
    {
        return false;
    }


    void Task::await_resume() {
        log("task await_resume");
    }

//==========SpawnTask

    SpawnTask SpawnTaskPromise::get_return_object(){
        return {std::coroutine_handle<SpawnTaskPromise>::from_promise(*this)};
    }

    SpawnTask co_spawn(Task task)
    {
        co_await task;
        // help simpilfy the destructor of task
        //if(!task.coroutine_handle()) {
        //TINYASYNC_UNREACHABLE();
        //}
    }

//========== IoContext
    std::string ioe2str(epoll_event& evt)
    {
        std::string str;
        str += ((evt.events & EPOLLIN) ? "EPOLLIN " : "");;
        str += ((evt.events & EPOLLPRI) ? "EPOLLPRI " : "");
        str += ((evt.events & EPOLLOUT) ? "EPOLLOUT " : "");
        str += ((evt.events & EPOLLRDNORM) ? "EPOLLRDNORM " : "");
        str += ((evt.events & EPOLLRDBAND) ? "EPOLLRDBAND " : "");
        str += ((evt.events & EPOLLWRBAND) ? "EPOLLWRBAND " : "");
        str += ((evt.events & EPOLLMSG) ? "EPOLLMSG " : "");
        str += ((evt.events & EPOLLERR) ? "EPOLLERR " : "");
        str += ((evt.events & EPOLLHUP) ? "EPOLLHUP " : "");
        str += ((evt.events & EPOLLRDHUP) ? "EPOLLRDHUP " : "");
        str += ((evt.events & EPOLLEXCLUSIVE) ? "EPOLLEXCLUSIVE " : "");
        str += ((evt.events & EPOLLWAKEUP) ? "EPOLLWAKEUP " : "");
        str += ((evt.events & EPOLLONESHOT) ? "EPOLLONESHOT " : "");
        str += ((evt.events & EPOLLET) ? "EPOLLET " : "");
        return str;
    }

//======PostTask
    void PostTask::set_callback(callback_type ptr) {
        m_callback = ptr;
    }
    PostTask::callback_type PostTask::get_callback() {
        return m_callback;
    }

    void PostTask::call(){
        if(m_callback !=nullptr) {
            m_callback(this);
        }
    }

    void PostTask::set_ref_count_ptr(int * _count)  { m_ref_count = _count; }
    void PostTask::set_awaiter_ptr(ListNode * node) { m_awaiter_ptr = node; }
    int  PostTask::get_ref_count() const            { return *m_ref_count; }

    bool PostTask::invalid() { 
        return m_callback == nullptr || m_ref_count == nullptr || *m_ref_count <= 1; 
    }

    //TODO 这个接口不对,不应该手动的控制 count
    void PostTask::set_ref_count(int count){
        if( m_ref_count !=nullptr)
            *m_ref_count = count;
        else
            m_ref_count = new int(count);
    }

    PostTask::~PostTask() {
        if( *m_ref_count <= 1) 
            delete m_ref_count;
        else
            --*m_ref_count;
    }

    ListNode * PostTask::get_m_awater_ptr() const {
        return m_awaiter_ptr;
    }

//==========IoContext

    IoContext::IoContext(){

        auto fd = epoll_create1(EPOLL_CLOEXEC); //创建一个epoll
        if (fd == -1)
        {
            //TODO better throw
            throw std::runtime_error("IoContext().IoContext(): can't create epoll");
        }
        m_epoll_handle = fd;
        log("event poll created", handle_c_str(m_epoll_handle));


        //fd = eventfd(1, EFD_NONBLOCK);
        //if (fd == -1)
        //{
            //throw("IoContext().IoContext(): can't create eventfd");
        //}

        //m_wakeup_handle = fd;
        //TINYASYNC_LOG("wakeup handle created %s", handle_c_str(m_wakeup_handle));

        //epoll_event evt;
        //evt.data.ptr = (void *)1;
        //evt.events = EPOLLIN | EPOLLONESHOT;
        //if(epoll_ctl(m_epoll_handle, EPOLL_CTL_ADD, m_wakeup_handle, &evt) < 0) {
            //std::string err =  format("can't set wakeup event %s (epoll %s)", handle_c_str(m_wakeup_handle), handle_c_str(m_epoll_handle));
            //TINYASYNC_LOG(err.c_str());
            ////throw_errno(err);
            //throw(err);
        //}

    }
    IoContext::IoContext(NativeHandle epoll_fd)
        :m_epoll_handle(epoll_fd)
    {
        log("m_epoll_handle",epoll_fd);
        //log("event poll created", handle_c_str(m_epoll_handle));
    }

    IoContext::~IoContext(){
        log("===== ~IoContext");
        ::close(m_epoll_handle);
    }

    void IoContext::post_task(PostTask * callback){
        m_task_queue.push(get_node(callback));
    }

    NativeHandle IoContext::event_poll_handle() const
    {
        return  m_epoll_handle;
    }

    void IoContext::run() {
        Callback *const CallbackGuard = (Callback *)8;
        for(;;) {
            // ===== 进行 注册任务的检查
            //遍历任务
            log(">>>>> check m_task_queue ");
            ListNode * end = m_task_queue.m_tail;
            ListNode * node_ptr  = m_task_queue.pop();

            for( ; node_ptr != end ; node_ptr = m_task_queue.pop() ){
                //转成
                log("node_ptr",node_ptr,"end",end);
                auto post_task = from_node_to_post_task(node_ptr);
                try  {
                    //post_task->get_callback()(post_task);
                    post_task->call(); //执行
                }
                catch(...){
                    log("catch io_context post_task error!");
                }
            }
            if( end !=nullptr) {
                log("call end ptr:",end);
                try  {
                    auto post_task = from_node_to_post_task(end);
                    post_task->call(); //执行
                }
                catch(...){
                    log("catch io_context post_task error!");
                }
            }


            // ===== 进行epoll event的检查
            const int maxevents = 5;
            epoll_event events[maxevents];
            int const timeout = 0; // indefinitely

            //TINYASYNC_LOG("waiting event ... handle = %s", handle_c_str(epfd));
            int nfds = epoll_wait(m_epoll_handle, (epoll_event *)events, maxevents, timeout);

            //log( "epoll_wait nfds:",nfds);
            for (auto i = 0; i < nfds; ++i)
            {
                auto &evt = events[i];
                log("event fd",evt.data.fd," events: ",ioe2str(evt));
                TINYASYNC_LOG("event %d of %d", i, nfds);
                TINYASYNC_LOG("event = %x (%s)", evt.events, ioe2str(evt).c_str());
                auto callback = (Callback *)evt.data.ptr;
                log("invoke callback");
                callback->callback(evt);
                //if (callback >= CallbackGuard)
                //{
                    //TINYASYNC_LOG("invoke callback");
                    //try
                    //{
                    //}
                    //catch (...)
                    //{
                        ////terminate_with_unhandled_exception();
                        //throw std::runtime_error("error in IoContext run");
                    //}
                //}
            }

        } // end for(;;)
    }

    
//==========Acceptor
    Address::Address(uint32_t v32)
    {
        m_int4 = htonl(v32);
        m_address_type = AddressType::IpV4;
    }

    Address::Address()
    {
        if(INADDR_ANY != 0) {
            m_int4 = htonl(INADDR_ANY);
        } else {
            m_int4 = 0;
        }
        m_address_type = AddressType::IpV4;
    }

    std::string Address::to_string() const 
    {
        char buf[256];
        if (m_address_type == AddressType::IpV4) {
            inet_ntop(AF_INET, &m_addr4, buf, sizeof(buf));
            return buf;
        } else if (m_address_type == AddressType::IpV6) {
            inet_ntop(AF_INET6, &m_addr6, buf, sizeof(buf));
            return buf;
        }
        return "";
    }

//=== endpoint

    Endpoint::Endpoint(Address address, uint16_t port)
    {
        m_address = address;
        m_port = port;
    }

    Endpoint::Endpoint() : Endpoint(Address(), 0)
    {
    }


//========== acceptor

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

//========== Connection
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
