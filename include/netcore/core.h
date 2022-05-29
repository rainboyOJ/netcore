/**
 * 使用socket + c++20 coroutines 实现一个简易的协程socket库
 */


#pragma once
#include "basic.hpp"
#include "define.h"

namespace netcore {

using namespace std::chrono_literals;

struct Task;
struct SpawnTask;
struct SpawnTaskPromise;

struct TaskPromise {

    using handle_type = std::coroutine_handle<TaskPromise>;

    handle_type m_h;

    TaskPromise() = default;
    TaskPromise(TaskPromise const & ) = delete;
    TaskPromise(TaskPromise && ) = delete;

    Task get_return_object();

    std::suspend_always initial_suspend() { return {}; }
    std::suspend_never  final_suspend() noexcept { return {}; }
    void unhandled_exception() { std::rethrow_exception(std::current_exception()); }

    void return_void() {};
    handle_type coroutine_handle() const { return m_h; }
};

struct Task {
    public:
        using promise_type = TaskPromise;
        using coroutine_handle_type = promise_type::handle_type;
    private:
        coroutine_handle_type m_h;
    public:

        Task(coroutine_handle_type h);

        coroutine_handle_type coroutine_handle();
        promise_type& promise();

        //awaitable
        bool await_ready() noexcept;

        template<typename Promise>
        auto await_suspend(std::coroutine_handle<Promise> awaiting_coro)
            ->coroutine_handle_type
        {
            return m_h;
        }

        void await_resume();
};

//============SpawnTask 
struct SpawnTaskPromise {
    std::suspend_never initial_suspend() { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }
    void unhandled_exception() {}
    SpawnTask get_return_object();
    void return_void() { }
};

struct SpawnTask
{
    using promise_type = SpawnTaskPromise;
    SpawnTask(std::coroutine_handle<> h) : m_handle(h)
    {}
    std::coroutine_handle<> m_handle;
};

SpawnTask co_spawn(Task task);

//========== IoContext
std::string ioe2str(epoll_event& evt);

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
    ~PostTask();

    void set_callback(callback_type ptr);
    callback_type get_callback();
    void call();
    void set_ref_count_ptr(int * _count);
    void set_awaiter_ptr(ListNode * node);
    bool invalid();

    void set_ref_count(int count);
    int get_ref_count() const;
    ListNode * get_m_awater_ptr() const ;

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
    IoContext();
    IoContext(NativeHandle epoll_fd); //使用其它 提供的epollfd
    ~IoContext();
    NativeHandle event_poll_handle() const;
    //加入任务
    void post_task(PostTask * callback);
    // 执行任务循环
    void run();
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
    Address(uint32_t v32);

    // if we bind on this address,
    // We will listen to all network card(s)
    Address();

    std::string to_string() const;

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
    Endpoint(Address address, uint16_t port);
    Endpoint();

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

void setnonblocking(NativeSocket fd);
void bind_socket(NativeSocket socket, Endpoint const& endpoint);
NativeSocket open_socket(Protocol const& protocol, bool blocking = false);

struct Acceptor;
struct Connection;

long long recv(NativeSocket sock,char *buf,std::size_t buff_size);
//发送数据,返回发送数据大小
long long send(NativeHandle sock,char *buf,long long send_size);

// ================ callback

struct ConnCallBack : public CallbackImplBase{
    Connection * m_connection;

    ConnCallBack(Connection * conn) 
        : CallbackImplBase(this), m_connection{conn}
    {}
    void on_callback(IoEvent& evt);
};

// ================ awaiter
struct AsyncIoWaiterBases {
    public:

    ListNode  m_node;
    Connection * m_conn{nullptr};
    std::size_t m_tranfer_bytes{0};
    char * m_buff;
    std::size_t m_buff_size;
    std::coroutine_handle<TaskPromise> m_h;
    std::chrono::system_clock::time_point m_end_time;
    PostTask * m_post_ptr;


    AsyncIoWaiterBases(Connection * conn,char * buff,std::size_t buff_size,std::chrono::seconds time_);

    static AsyncIoWaiterBases *from_node(ListNode *node) {
        return (AsyncIoWaiterBases*)((char*)node - offsetof(AsyncIoWaiterBases, m_node));
    }
    bool await_ready() { return false; }

    static void check_deley(PostTask * task_ptr);

    //template<class Promise>
    //inline void await_suspend(std::coroutine_handle<Promise> suspend_coroutine) {
        //auto h = suspend_coroutine.promise().coroutine_handle();
        //await_suspend(h);
    //}

    ~AsyncIoWaiterBases();

    bool expired();
};

struct AsyncReadAwaiter :public AsyncIoWaiterBases {
    //using AsyncIoWaiterBases::AsyncIoWaiterBases;

    AsyncReadAwaiter(Connection * conn,char * buff,std::size_t buff_size,std::chrono::seconds time_) ;

    bool await_suspend(std::coroutine_handle<TaskPromise> h );
    std::size_t await_resume();
};

struct AsyncSendAwaiter :public AsyncIoWaiterBases {

    char * buf(); //发送数据的位置
    std::size_t left_buf_size();
    void update_sent_size(std::size_t siz); //更新已经发送的数据大小
    bool finised(); //数据是否发送完毕

    using AsyncIoWaiterBases::AsyncIoWaiterBases;
    bool await_suspend(std::coroutine_handle<TaskPromise> h );
    std::size_t await_resume();
};

// ================ Connection
class Connection {
        friend AsyncIoWaiterBases;
        friend AsyncReadAwaiter;
        friend AsyncSendAwaiter;
        friend ConnCallBack;
    private:
        NativeSocket m_socket{-1};
        //IoContext * m_ctx{nullptr};

        //bool m_recv_shutdown = false;
        //bool m_send_shutdown = false;
        //bool m_tcp_nodelay   = false;

        bool m_added_to_event_pool = false;

        AsyncReadAwaiter * m_read_awaiter{nullptr};
        AsyncSendAwaiter * m_send_awaiter{nullptr};
        ConnCallBack m_callback{this};

        std::exception_ptr m_except{nullptr}; //存异常

    public:
        IoContext * m_ctx{nullptr};
        using CONN_PTR = std::unique_ptr<Connection>;
        
        bool m_send_at_awaiter = false; // 是否需要在awaiter 里写
        bool m_recv_at_awaiter = false; // 是否需要在awaiter 里写

        Connection(IoContext * _ctx ,NativeHandle socket_fd)
            :m_ctx{_ctx},m_socket{socket_fd}
        {
            registerToIoCtx();
        }

        Connection(Connection &&);
        void operator=(Connection &&);

        Connection(Connection const &) = delete;
        void operator=(Connection const &) = delete;

        void clear_except();

        ~Connection();

        /**
            * !!注意 async_read和async_send 都不支持在多个同时在多个携程内调用
            */
        // retvalue=0 表示连接断开了
        AsyncReadAwaiter async_read(char * buf,std::size_t buff_size,std::chrono::seconds time_ = 10s);
        // retvalue=0 表示连接断开了
        AsyncSendAwaiter async_send(char * buf,std::size_t buff_size,std::chrono::seconds time_ = 10s);
        void close();
        //static void on_callback(IoEvent & evt);

    private:
        void registerToIoCtx();
        void unRegisterToIoCtx();
};
    
//========== Acceptor
class AcceptorCallback : public CallbackImplBase { 
    friend class Acceptor;
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
