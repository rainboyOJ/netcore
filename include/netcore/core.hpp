/**
 * 使用socket + c++20 coroutines 实现一个简易的协程socket库
 */

#pragma once
#include "basic.hpp"
#include "define.h"

namespace netcore {

using namespace std::chrono_literals;

struct Task;

//============SpawnTask 

struct SpawnTask
{

    struct SpawnTaskPromise {
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { 
            //std::cout << "spawnTask promise destroy" << std::endl;
            return {}; 
        }
        void unhandled_exception() {}

        SpawnTask get_return_object() {
            m_h  =  std::coroutine_handle<SpawnTaskPromise>::from_promise(*this);
            return {m_h};
            //return {std::coroutine_handle<SpawnTaskPromise>::from_promise(*this)};

        }
        std::coroutine_handle<SpawnTaskPromise> m_h;

        void return_void() { }
    };

    using promise_type = SpawnTaskPromise;
    using handle_type = std::coroutine_handle<>;

    SpawnTask(handle_type h) : m_handle(h)
    {}

    handle_type m_handle;
};


//============SpawnTask 

struct Task {
    public:
        int id{0};
        struct TaskPromise {
            using handle_type = std::coroutine_handle<TaskPromise>;
            handle_type m_h;
            SpawnTask::handle_type m_countinue;

            TaskPromise() = default;
            TaskPromise(TaskPromise const & ) = delete;
            TaskPromise(TaskPromise && ) = delete;

            Task get_return_object(){
                this->m_countinue = std::noop_coroutine();
                m_h = std::coroutine_handle<TaskPromise>::from_promise(*this);
                return { m_h };
            }

            std::suspend_always initial_suspend() { return {}; }
            std::suspend_always  final_suspend() noexcept { 
                if( m_countinue )
                    m_countinue.resume();
                return {}; 
            }

            void unhandled_exception() { 
                std::rethrow_exception(std::current_exception()); 
            }

            void return_void() {};

            handle_type coroutine_handle() const { return m_h; }
        };
        using promise_type = TaskPromise;
        using coroutine_handle_type = promise_type::handle_type;

    private:
        coroutine_handle_type m_h;

    public:

        Task(coroutine_handle_type h) :m_h{h}
        {}
        Task(Task const &t )
            : m_h {t.m_h}
        {
            id = t.id-1;
        }

        ~Task() {
            //std::cout << "~Task id : " << id << std::endl;
            //if( m_h )
                //m_h.destroy();
        }
        void clear_coro_handle(){
            m_h = {};
        }

        coroutine_handle_type coroutine_handle() {
            return m_h;
        }

        promise_type& promise() {
            return m_h.promise();
        }

        //awaitable
        bool await_ready() noexcept { return false; }

        //如果await_suspend返回一个coroutine_handle,会handle 执行 reseume,然后挂起自己
        //template<typename Promise>
        auto await_suspend(SpawnTask::handle_type awaiting_coro)
            ->coroutine_handle_type
        {
            m_h.promise().m_countinue = awaiting_coro;
            //std::cout << "tast await_suspend" << std::endl;
            return m_h;
        }

        void await_resume() {
            //LOG(INFO)  << "task await_resume";
        }
        
};

inline SpawnTask co_spawn(Task task) {
    //std::cout << "co_spawn start" << std::endl;
    co_await task; //TODO ? 那么 会不会这个task 执行完呢?
    if( task.coroutine_handle() )
        task.coroutine_handle().destroy();
}


//========== IoContext
inline std::string ioe2str(epoll_event& evt) {
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

class IoContext;

/**
 * @desc 加入到IoCtx中的任务队列中的任务的封装
 */
class PostTask
{
friend IoContext;
public:
    // your callback
    using callback_type = void (*)(PostTask *);
    PostTask() = default;
    ~PostTask() {}

    void set_callback(callback_type ptr) {
        m_callback = ptr;
    }

    callback_type get_callback(){
        return m_callback;
    }
    void call(){
        if(m_callback !=nullptr) {
            m_callback(this);
        }
    }
    void set_awaiter_ptr(ListNode * node) { m_awaiter_ptr = node; }
    bool invalid() {
        return m_callback == nullptr || m_ref_count <= 0; 
    }

    void set_ref_count(int count) { m_ref_count = count;}
    int get_ref_count() const { return  m_ref_count; }
    ListNode * get_m_awater_ptr() const  { return m_awaiter_ptr;}

private:
    int m_ref_count {0};
    friend class IoCtxBase;
    // use internally
    ListNode m_node;
    ListNode * m_awaiter_ptr{nullptr};
    callback_type m_callback{nullptr};

};

/**
 * @desc epoll事件中心
 */
class IoContext {
private:
    NativeHandle m_epoll_handle = NULL_HANDLE;
    Queue m_task_queue;
    PostTask check_expire_awaiter; // 检查过期awaiter
    std::vector<ListNode> m_awaiters; //指向awaiter
    
    static PostTask *from_node_to_post_task(ListNode *node) {
        //return (PostTask *)((char *)node - offsetof(PostTask, m_node));
        return (PostTask *)((char *)node - offset_of_impl::offsetOf(&PostTask::m_node));
    }

    static ListNode *get_node(PostTask *posttask) {
        return &posttask->m_node;
    }


public:
    IoContext() {

        auto fd = epoll_create1(EPOLL_CLOEXEC); //创建一个epoll
        if (fd == -1)
        {
            //TODO better throw
            throw std::runtime_error("IoContext().IoContext(): can't create epoll");
        }
        m_epoll_handle = fd;
        LOG(INFO) << "event poll created : " <<handle_c_str(m_epoll_handle);

    }

    //使用其它 提供的epollfd
    IoContext(NativeHandle epoll_fd) 
        :m_epoll_handle(epoll_fd)
    {}


    ~IoContext() {
        ::close(m_epoll_handle);
    }

    NativeHandle event_poll_handle() const {
        return  m_epoll_handle;
    }

    //加入任务
    void post_task(PostTask * callback) {
        m_task_queue.push(get_node(callback));
    }
    // 执行任务循环
    void run() {
        Callback *const CallbackGuard = (Callback *)8;
        for(;;) {
            // ===== 进行 注册任务的检查
            //遍历任务
            //log(">>>>> check m_task_queue ");
            ListNode * end = m_task_queue.m_tail;
            ListNode * node_ptr  = m_task_queue.pop();

            for( ; node_ptr != end ; node_ptr = m_task_queue.pop() ){
                //转成
                auto post_task = from_node_to_post_task(node_ptr);
                try  {
                    //post_task->get_callback()(post_task);
                    post_task->call(); //执行
                }
                catch(...){
                    LOG(ERROR) << "catch io_context post_task error!";
                }
            }
            if( end !=nullptr) {
                //log("call end ptr:",end);
                try  {
                    auto post_task = from_node_to_post_task(end);
                    post_task->call(); //执行
                }
                catch(...){
                    LOG(ERROR) << "catch io_context post_task error!";
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
                LOG(INFO) << "event fd:[" <<  evt.data.fd << "],events: " << ioe2str(evt);
                auto callback = (Callback *)evt.data.ptr;
                //log("invoke callback");
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
};

//==========Acceptor
struct Protocol
{
    static Protocol ip_v4()
    {
        return {};
    }
};

enum class AddressType {
    IpV4,
    IpV6,
};

//对地址的封装
struct Address
{

    // ip_v32 host byte order
    Address(uint32_t v32) {
        m_int4 = htonl(v32);
        m_address_type = AddressType::IpV4;
    }

    // if we bind on this address,
    // We will listen to all network card(s)
    Address() {
        if(INADDR_ANY != 0) {
            m_int4 = htonl(INADDR_ANY);
        } else {
            m_int4 = 0;
        }
        m_address_type = AddressType::IpV4;
    }

    std::string to_string() const{
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

    static Address Any()
    {
        return {};
    }

    union
    {
        uint32_t m_int4;
        in_addr m_addr4;
        in6_addr m_addr6;
    };
    AddressType m_address_type;
};


struct Endpoint
{
    Endpoint(Address address, uint16_t port){
        m_address = address;
        m_port = port;
    }

    Endpoint() : Endpoint(Address(), 0) 
    {}


    Address address() const noexcept {
        return m_address;
    }

    uint16_t port() const noexcept { 
        return m_port;
    }

    private:
    Address m_address;
    uint16_t m_port;
};

inline void setnonblocking(NativeSocket fd) {
    //int status = fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
    int status = fcntl(fd, F_SETFL, O_NONBLOCK);

    if (status == -1) {
        throw std::system_error(errno, std::system_category(), "can't set fd nonblocking");
    }
}

inline void bind_socket(NativeSocket socket, Endpoint const& endpoint) {
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

inline NativeSocket open_socket(Protocol const& protocol, bool blocking = false) {
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
    return socket_;
}

struct Acceptor;

inline long long recv(NativeSocket sock,char *buf,std::size_t buff_size){
    //log("recv....");
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
//发送数据,返回发送数据大小
inline long long send(NativeHandle sock,char * buf,long long send_size){
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

// ================ callback


// ================ awaiter
template<typename Connection>
struct AsyncIoWaiterBases {
public:

    ListNode  m_node;
    Connection * m_conn{nullptr};
    std::size_t m_tranfer_bytes{0};
    char * m_buff;
    std::size_t m_buff_size;
    std::coroutine_handle<Task::TaskPromise> m_h;
    std::chrono::system_clock::time_point m_end_time;
    PostTask * m_post_ptr;


    AsyncIoWaiterBases(Connection * conn,char * buff,std::size_t buff_size,std::chrono::seconds time_)
        : m_conn{conn},m_buff{buff},m_buff_size{buff_size}
    {
        m_end_time = std::chrono::system_clock::now() + time_;
        //创建一个全局的PostTask
        m_post_ptr = new PostTask;
        m_post_ptr->set_callback(check_deley);
        m_post_ptr->set_ref_count(1); //设置1表示这个awaiter还存在
        m_post_ptr->set_awaiter_ptr(&m_node);
        //加入队列中
        m_conn->m_ctx->post_task(m_post_ptr);
    }

    static AsyncIoWaiterBases *from_node(ListNode *node) {
        return (AsyncIoWaiterBases*)((char*)node - offsetof(AsyncIoWaiterBases, m_node));
    }
    bool await_ready() { return false; }

    static void check_deley(PostTask * task_ptr)
    {
        // ref_count 是否为0
        if(task_ptr->invalid()) {
            delete task_ptr; //删除task
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
                //log("add",task_ptr,"to ctx queue");
            }
        }
    }

    //template<class Promise>
    //inline void await_suspend(std::coroutine_handle<Promise> suspend_coroutine) {
        //auto h = suspend_coroutine.promise().coroutine_handle();
        //await_suspend(h);
    //}

    ~AsyncIoWaiterBases() {
        m_post_ptr->set_ref_count(0); //表示awaiter不存在了
    }

    bool expired() {
        return  std::chrono::system_clock::now() > m_end_time;
    }
};

template<typename Connection>
struct AsyncReadAwaiter :public AsyncIoWaiterBases<Connection> {
    //using AsyncIoWaiterBases::AsyncIoWaiterBases;
    using AsyncIoWaiterBases<Connection>::m_h;
    using AsyncIoWaiterBases<Connection>::m_conn;
    using AsyncIoWaiterBases<Connection>::m_tranfer_bytes;

    AsyncReadAwaiter(Connection * conn,char * buff,std::size_t buff_size,std::chrono::seconds time_) 
            :AsyncIoWaiterBases<Connection>(conn,buff,buff_size,time_)
    {}

    bool await_suspend(std::coroutine_handle<Task::TaskPromise> h ) {

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

    std::size_t await_resume(){
        m_conn->m_read_awaiter = nullptr;
        if( m_conn->m_except != nullptr) {
            std::rethrow_exception(m_conn->m_except);
        }
        return m_tranfer_bytes;
    }
};

template<typename Connection>
struct AsyncSendAwaiter :public AsyncIoWaiterBases<Connection> {
    using AsyncIoWaiterBases<Connection>::m_h;
    using AsyncIoWaiterBases<Connection>::m_buff;
    using AsyncIoWaiterBases<Connection>::m_buff_size;
    using AsyncIoWaiterBases<Connection>::m_conn;
    using AsyncIoWaiterBases<Connection>::m_tranfer_bytes;

    using AsyncIoWaiterBases<Connection>::AsyncIoWaiterBases;

    //发送数据的位置
    char * buf()
    {
        return m_buff + m_tranfer_bytes;
    }

    std::size_t left_buf_size()
    {
        return m_buff_size - m_tranfer_bytes;
    }
    //更新已经发送的数据大小
    void update_sent_size(std::size_t siz)
    {
        m_tranfer_bytes+= siz;
    }
    //数据是否发送完毕
    bool finised()
    {
        return m_tranfer_bytes>= m_buff_size;
    }

    bool await_suspend(std::coroutine_handle<Task::TaskPromise> h )
    {
        m_h = h;
        m_conn->m_send_awaiter = this;
        if( m_conn -> m_send_at_awaiter ) {
            //log("AsyncSendAwaiter await_suspend send");
            try {
                long long nbytes  = netcore::send(m_conn->m_socket, this->buf(), this->left_buf_size());
                this->update_sent_size(nbytes);
            }
            catch(...){
                m_conn->m_except = std::current_exception();
                //log("catch error set m_conn m_except");
                return false;
            }
            if(this->finised()) { //发送完毕
                return false;
            }
        }
        return true;
    }

    std::size_t await_resume() {
        m_conn->m_send_awaiter = nullptr;
        if( m_conn->m_except != nullptr) {
            std::rethrow_exception(m_conn->m_except);
        }
        return m_tranfer_bytes;
    }
};

// ================ Connection
class RawConnection {
public:
        struct ConnCallBack : public CallbackImplBase{
            RawConnection * m_connection;

            ConnCallBack(RawConnection * conn) 
                : CallbackImplBase(this), m_connection{conn}
            {}
            void on_callback(IoEvent& evt) 
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
                    //log("EVENt run RECV >>>>");
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
        };

        friend AsyncIoWaiterBases<RawConnection>;
        friend AsyncReadAwaiter<RawConnection>;
        friend AsyncSendAwaiter<RawConnection>;
        friend ConnCallBack;


    private:
        NativeSocket m_socket{-1};
        //IoContext * m_ctx{nullptr};

        bool m_added_to_event_pool = false;

        AsyncReadAwaiter<RawConnection> * m_read_awaiter{nullptr};
        AsyncSendAwaiter<RawConnection> * m_send_awaiter{nullptr};
        ConnCallBack m_callback{this};

        std::exception_ptr m_except{nullptr}; //存异常

    public:
        IoContext * m_ctx{nullptr};
        using CONN_PTR = std::unique_ptr<RawConnection>;
        
        bool m_send_at_awaiter = false; // 是否需要在awaiter 里写
        bool m_recv_at_awaiter = false; // 是否需要在awaiter 里写

        RawConnection(IoContext * _ctx ,NativeHandle socket_fd)
            :m_ctx{_ctx},m_socket{socket_fd}
        {
            registerToIoCtx();
        }


        RawConnection(RawConnection const &) = delete;
        void operator=(RawConnection const &) = delete;

        void clear_except() {
            this->m_except = nullptr;
        }

        ~RawConnection() {
            LOG(INFO) << ("Connection deconsructor");
            close();
            if( m_except != nullptr) {
                std::rethrow_exception(m_except);
            }
        }

        /**
        * !!注意 async_read和async_send 都不支持在多个同时在多个携程内调用
        */
        // retvalue=0 表示连接断开了
        AsyncReadAwaiter<RawConnection> async_read(char * buf,std::size_t buff_size,std::chrono::seconds time_ = 10s){
            return {this,buf,buff_size,time_};
        }
        // retvalue=0 表示连接断开了
        AsyncSendAwaiter<RawConnection> async_send(char * buf,std::size_t buff_size,std::chrono::seconds time_ = 10s) {

            return {this,buf,buff_size,time_};
        }
        void close() {
            LOG(INFO) << ("connection close");
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
        //static void on_callback(IoEvent & evt);

    private:
        void registerToIoCtx() {
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
        }
        void unRegisterToIoCtx() { /*TODO*/}
};

using ConnCallBack = RawConnection::ConnCallBack;


//========== Acceptor
template<typename Acceptor>
struct AcceptorAwaiter {

    ListNode  m_node;
    Acceptor* m_acceptor;
    NativeSocket m_conn_socket;
    std::coroutine_handle<Task::TaskPromise> m_suspend_coroutine;

public:

    static AcceptorAwaiter *from_node(ListNode *node) {
        return (AcceptorAwaiter *)((char*)node - offsetof(AcceptorAwaiter, m_node));
    }

    bool await_ready() { return false; }

    AcceptorAwaiter(Acceptor& acceptor) 
        :m_acceptor(&acceptor)
    {}


    template<class Promise>
    inline void await_suspend(std::coroutine_handle<Promise> suspend_coroutine) {
        auto h = suspend_coroutine.promise().coroutine_handle();
        await_suspend(h);
    }

    /**
     * 将 Acceptor的m_socket加入到epoll里监听,设置回调用函数
     */
    bool await_suspend(std::coroutine_handle<Task::TaskPromise> h)
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
                //log("=====================add epoll ctl error,epoll_fd",std::to_string(epfd),"listenfd",std::to_string(listenfd));
                //throw_errno(format("can't await accept %x (epoll %s)", socket_c_str(listenfd), handle_c_str(epfd)));
                throw std::runtime_error("add epoll ctl error");
            }
            //log("listenfd",listenfd,"add epoll",std::to_string(epfd),"succ");
            m_acceptor->m_added_to_event_pool = true;
        }
        return true;
    }

    RawConnection::CONN_PTR await_resume()
    {
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
        return  std::make_unique<RawConnection>(acceptor->m_ctx,conn_sock);
    }
};
    
struct Acceptor {
    public:
        //========== Acceptor
        class __AcceptorCallback : public CallbackImplBase { 
            friend class Acceptor;
            Acceptor* m_acceptor;
            public:
            __AcceptorCallback(Acceptor* acceptor) 
                : CallbackImplBase(this), m_acceptor { acceptor }
            {}

            void on_callback(IoEvent& evt){
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
                    LOG(INFO) <<  "acceptor connection : " <<  conn_sock;
                    AcceptorAwaiter<Acceptor> * awaiter = AcceptorAwaiter<Acceptor>::from_node(node);
                    awaiter->m_conn_socket = conn_sock;
                    //TINYASYNC_RESUME(awaiter->m_suspend_coroutine);
                    awaiter->m_suspend_coroutine.resume();
                } else {
                    // this will happen when after accetor.listen() but not yet co_await acceptor.async_accept()
                    // you should have been using level triger to get this event the next time
                    LOG(INFO) << "No awaiter found, event ignored";
                }
            }
        };

    private:
        friend AcceptorAwaiter<Acceptor>;

        IoContext * m_ctx;
        Protocol     m_protocol;
        Endpoint     m_endpoint;
        NativeSocket m_socket;
        bool m_added_to_event_pool;

        __AcceptorCallback m_callback = this;
        Queue m_awaiter_que; //存awaiter的queue
    public:
        Acceptor() {
            m_ctx = nullptr;
            m_socket = NULL_SOCKET;
            m_added_to_event_pool = false;
        }
        Acceptor(IoContext & ctx) {
            m_ctx = &ctx;
            m_socket = NULL_SOCKET;
            m_added_to_event_pool = false;
        }


        Acceptor(IoContext& ctx, Protocol protocol, Endpoint endpoint)
            :Acceptor(ctx)
        {
            init(&ctx, protocol, endpoint);
        }

        Acceptor(Protocol protocol, Endpoint endpoint) 
            :Acceptor()
        {
            init(nullptr,protocol,endpoint);
        }

        //初始化构造
        void init(IoContext *, Protocol const& protocol, Endpoint const& endpoint) 
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
                //log("listenfd is ",m_socket);
            }
            catch(...){
                reset();
                auto what = format("open/bind/listen failed %s:%d", endpoint.address().to_string().c_str(), (int)(unsigned)endpoint.port());
                //std::throw_with_nested(std::runtime_error(what));
                throw std::runtime_error(what);
            }
        }

        //重置
        void reset_io_context(IoContext &ctx, Acceptor &r){
            m_ctx = &ctx;
            m_protocol = r.m_protocol;
            m_endpoint = r.m_endpoint;
            m_socket = r.m_socket;
            m_added_to_event_pool = false;
        }

        //监听
        void listen(){
            int max_pendding_connection = 5;// TODO change this
            int err = ::listen(m_socket, max_pendding_connection);
            LOG(INFO) << "listen at " << m_endpoint.port();
            if (err == -1) {
                throw std::runtime_error("can't listen socket");
            }
        }

        // 异步的接收连接
        AcceptorAwaiter<Acceptor> async_accept() {
            return {*this};
        }

        void reset()
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
                if(close_socket(m_socket) < 0) {
                    printf("%d\n", errno);
                    std::exit(1);
                }
                m_socket = NULL_SOCKET;
            }
        }
};
using AcceptorCallback = Acceptor::__AcceptorCallback;



} // end namespace netcore
