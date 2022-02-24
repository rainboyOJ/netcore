#pragma once

#include <unordered_map>
#include <fcntl.h>       // fcntl()
#include <unistd.h>      // close()
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <functional>
#include <memory>

#include "threadpool.h"
#include "sqlconnpool.h"
#include "epoller.h"
#include "heaptimer.h"

namespace rojcpp {
    
template<typename HttpConn>
class epoll_server {
public:
    epoll_server()=default;
    void __init__(
        int port, int trigMode, int timeoutMS, bool OptLinger, 
        int sqlPort, const char* sqlUser, const  char* sqlPwd, 
        const char* dbName, int connPoolNum, int threadNum,
        bool openLog, int logLevel, int logQueSize
            );

    ~epoll_server();
    virtual void Start() = 0; //启动
    using connShrPtr = std::shared_ptr<HttpConn>; //connection的类型

protected:
    bool InitSocket_();  //创建监听socket 并开始listen,加入到epoll里
    void InitEventMode_(int trigMode);
    //template<typename... U>
    //void AddClient_(int fd, sockaddr_in addr,U&&... args); //加一个client
  
    //有新的client来了 把它加入
    virtual void DealListen_() = 0;
    //当可以写的时候，写入
    void DealWrite_(HttpConn* client);
    //读取 来的数据
    void DealRead_(HttpConn* client);

    //发送错误
    void SendError_(int fd, const char*info);
    void ExtentTime_(HttpConn* client); // 拓展对应Fd的时间
    void CloseConn_(HttpConn* client);  // 关闭连接

    void OnRead_(HttpConn* client); // 读取client的数据
    void OnWrite_(HttpConn* client);
    void OnProcess(HttpConn* client); //?

    static const int MAX_FD = 65536;

    static int SetFdNonblock(int fd);

    int port_;
    bool openLinger_;
    int timeoutMS_;  /* 毫秒MS */
    bool isClose_;
    int listenFd_;
    //char* srcDir_;
    
    uint32_t listenEvent_;
    uint32_t connEvent_;
   
    std::unique_ptr<HeapTimer> timer_;       //超时控制器
    std::unique_ptr<ThreadPool> threadpool_; //线程池
    std::unique_ptr<Epoller> epoller_;       //epoll封装
    std::unordered_map<int, std::shared_ptr<HttpConn> > users_; // connection 与 SocketFd 的映射
};


template<typename HttpConn>
void epoll_server<HttpConn>::__init__(
        int port, int trigMode, int timeoutMS, bool OptLinger, 
        int sqlPort, const char* sqlUser, const  char* sqlPwd, 
        const char* dbName, int connPoolNum, int threadNum,
        bool openLog, int logLevel, int logQueSize){

    port_       = port;
    openLinger_ = OptLinger;
    timeoutMS_  = timeoutMS;
    isClose_    = false;

    timer_      = std::make_unique<HeapTimer>(); 
    threadpool_ = std::make_unique<ThreadPool>(threadNum);
    epoller_    = std::make_unique<Epoller>();

    //TODO HttpConn init
    SqlConnPool::Instance()->Init("127.0.0.1", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);

    InitEventMode_(trigMode);
    if(!InitSocket_()) { isClose_ = true;}
    if(openLog) {
#ifdef DEBUG
        Log::Instance()->init_default();
#else
        Log::Instance()->init(logLevel, "./log", ".log", logQueSize);
#endif
        if(isClose_) { LOG_ERROR("========== Server init error!=========="); }
        else {
            LOG_INFO("========== Server init ==========");
            LOG_INFO("Port:%d, OpenLinger: %s", port_, OptLinger? "true":"false");
            LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
                            (listenEvent_ & EPOLLET ? "ET": "LT"),
                            (connEvent_ & EPOLLET ? "ET": "LT"));
            LOG_INFO("LogSys level: %d", logLevel);
            //LOG_INFO("srcDir: %s", HttpConn::srcDir);
            LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum, threadNum);
        }
    }
}

template<typename HttpConn>
epoll_server<HttpConn>::~epoll_server(){
    close(listenFd_);
    isClose_ = true;
    SqlConnPool::Instance()->ClosePool();
}

template<typename HttpConn>
void epoll_server<HttpConn>::InitEventMode_(int trigMode) {
    listenEvent_ = EPOLLRDHUP;
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP;
    switch (trigMode)
    {
    case 0:
        break;
    case 1:
        connEvent_ |= EPOLLET;
        break;
    case 2:
        listenEvent_ |= EPOLLET;
        break;
    case 3:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    default:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    }
    //HttpConn::isET = (connEvent_ & EPOLLET); // TODO
}


template<typename HttpConn>
bool epoll_server<HttpConn>::InitSocket_() {
    int ret;
    struct sockaddr_in addr;
    if(port_ > 65535 || port_ < 1024) {
        LOG_ERROR("Port:%d error!",  port_);
        return false;
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);
    struct linger optLinger = { 0 };
    if(openLinger_) {
        /* 优雅关闭: 直到所剩数据发送完毕或超时 */
        optLinger.l_onoff = 1;
        optLinger.l_linger = 1;
    }

    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if(listenFd_ < 0) {
        LOG_ERROR("Create socket error!", port_);
        return false;
    }

    ret = setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if(ret < 0) {
        close(listenFd_);
        LOG_ERROR("Init linger error!", port_);
        return false;
    }

    int optval = 1;
    /* 端口复用 */
    /* 只有最后一个套接字会正常接收数据。 */
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    if(ret == -1) {
        LOG_ERROR("set socket setsockopt error !");
        close(listenFd_);
        return false;
    }

    ret = bind(listenFd_, (struct sockaddr *)&addr, sizeof(addr));
    if(ret < 0) {
        LOG_ERROR("Bind Port:%d error!", port_);
        close(listenFd_);
        return false;
    }

    ret = listen(listenFd_, 6);
    if(ret < 0) {
        LOG_ERROR("Listen port:%d error!", port_);
        close(listenFd_);
        return false;
    }
    ret = epoller_->AddFd(listenFd_,  listenEvent_ | EPOLLIN);
    if(ret == 0) {
        LOG_ERROR("Add listen error!");
        close(listenFd_);
        return false;
    }
    SetFdNonblock(listenFd_);
    LOG_INFO("Server port:%d", port_);
    return true;
}


template<typename HttpConn>
void epoll_server<HttpConn>::SendError_(int fd, const char*info) {
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if(ret < 0) {
        LOG_WARN("send error to client[%d] error!", fd);
    }
    close(fd);
}

template<typename HttpConn>
void epoll_server<HttpConn>::CloseConn_(HttpConn* client) {
    //assert(client);
    LOG_INFO("Client[%d] quit!", client->GetFd());
    epoller_->DelFd(client->GetFd());
    client->Close();
}


//template<typename HttpConn>
//void epoll_server<HttpConn>::DealListen_() {
    //struct sockaddr_in addr;
    //socklen_t len = sizeof(addr);
    //do {
        //int fd = accept(listenFd_, (struct sockaddr *)&addr, &len);
        //if(fd <= 0) { return;}
        //else if(HttpConn::userCount >= MAX_FD) {
            //SendError_(fd, "Server busy!");
            //LOG_WARN("Clients is full!");
            //return;
        //}
        //AddClient_(fd, addr);
    //} while(listenEvent_ & EPOLLET);
//}

template<typename HttpConn>
void epoll_server<HttpConn>::DealRead_(HttpConn* client) {
    assert(client);
    ExtentTime_(client);
    threadpool_->AddTask(std::bind(&epoll_server<HttpConn>::OnRead_, this, client));
}


template<typename HttpConn>
void epoll_server<HttpConn>::DealWrite_(HttpConn* client) {
    assert(client);
    ExtentTime_(client);
    // TODO
    threadpool_->AddTask(std::bind(&epoll_server<HttpConn>::OnWrite_, this, client));
}

template<typename HttpConn>
void epoll_server<HttpConn>::ExtentTime_(HttpConn* client) {
    assert(client);
    if(timeoutMS_ > 0) { timer_->adjust(client->GetFd(), timeoutMS_); }
}


template<typename HttpConn>
void epoll_server<HttpConn>::OnRead_(HttpConn* client) {
    assert(client);
    int ret = -1;
    int readErrno = 0;
    ret = client->read(&readErrno); //调用client的read,读取所有的数据
    if(ret <= 0 && readErrno != EAGAIN) { //什么也没有读取到且不是EAGAIN
        CloseConn_(client); //关闭
        return;
    }
    OnProcess(client);
}

//这样保证了只有一个线程在同一个fd上工作
template<typename HttpConn>
void epoll_server<HttpConn>::OnProcess(HttpConn* client) {
    if(client->process()) { 
        if( client->is_ws_socket() ){ // websocket 如果进入写的状态就是等关闭
            epoller_->ModFd(client->GetFd(), connEvent_); //不会进入读取也不会进入写 ,等待WS_manager 的关闭
            return ;
        }
        // client->process返回true的时候表示已经读取完毕想要的数据
        // 转入 写的阶段,否则继续读取
        LOG_DEBUG("After client->process() set server continue to write.");
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
    } else {
        LOG_DEBUG("After client->process() set server continue to read.");
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN);
    }
}
template<typename HttpConn>
void epoll_server<HttpConn>::OnWrite_(HttpConn* client) {
    assert(client);
    int ret = -1;
    int writeErrno = 0;
    ret = client->write(&writeErrno);
    if(client->ToWriteBytes() == 0) {
        // 有可能一次写不完
        // 也有可能 读取->写->读->写 这样
        // 主要看 connection 的process 是如果工作的
        if( client->has_continue_workd() ) { 
            /* 继续传输 */
            LOG_DEBUG("client has_continue_workd,so process it");
            OnProcess(client);
            return;
        }
        /* 传输完成 */
        //是否保持长连接?
        LOG_DEBUG("trans ok IsKeepAlive %d",client->IsKeepAlive()); 
        if(client->IsKeepAlive()) {
            OnProcess(client);
            return;
        }
    }
    else if(ret < 0) {
        if(writeErrno == EAGAIN) {
            /* 继续传输 */
            epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
            return;
        }
    }
    CloseConn_(client);
}
template<typename HttpConn>
int epoll_server<HttpConn>::SetFdNonblock(int fd) {
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}


} // end namespace rojcpp
