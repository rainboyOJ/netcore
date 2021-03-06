#pragma once
#include <string>
#include <vector>
#include <string_view>

#include "define.h"
//#include "heaptimer.h"

#include "epoll_server.hpp" 
#include "utils.hpp" 
#include "connection.hpp"
#include "ws_before_ap.h"
#include "http_router.hpp"
#include "http_cache.hpp"
//#include "timer.hpp" //定时器
#include "hs_utils.h"

//#ifdef __USE_SESSION__ //暂时没有实现这个功能
//#include "session_manager.hpp" // 不在使用session_manager功能
#include "cookie.hpp"
//#endif


namespace netcore {
    
    //cache
    template<typename T>
    struct enable_cache {
        enable_cache(T t) :value(t) {}
        T value;
    };

    using SocketType = int;

    class http_server_ : private noncopyable ,public epoll_server<connection> {
    public:
        using type = int; // fd is int
        http_server_() {
            init(__config__::port //端口
                ,__config__::trigMode //int trigMode, 2表示监听的时候是 LET 模式
                ,__config__::timeoutMS//10*1000 //int timeoutMS 超时就会断开连接
                ,__config__::OptLinger //true //bool OptLinger, 优雅关闭: 直到所剩数据发送完毕或超时 ,__config__:://3306 //int sqlPort,
                //,__config__::sqlPort// sql的端口
                //,__config__::sqlUser//"root" //const char *sqlUser,
                //,__config__::sqlPwd//"root" //const char *sqlPwd,
                //,__config__::dbName//"netcore" //const char *dbName,
                //,__config__::connPoolNum//4//int connPoolNum,
                ,__config__::threadNum//4//int threadNum,
                ,__config__::openLog//true//bool openLog,
                ,__config__::logLevel//0//int logLevel, //最低级别
                ,__config__::logQueSize//1//int logQueSize
                );
        };

        void init(
        int port, int trigMode, int timeoutMS, bool OptLinger, 
        //int sqlPort, const char* sqlUser, const  char* sqlPwd, 
        //const char* dbName, int connPoolNum,
        int threadNum,
        bool openLog, int logLevel, int logQueSize
                )
        {
            epoll_server<connection>::__init__(port, trigMode, timeoutMS, OptLinger,
                    //sqlPort, sqlUser, sqlPwd, dbName, connPoolNum,
                    threadNum, openLog, logLevel, logQueSize);
            http_cache::get().set_cache_max_age(86400); // 最大的cache时间
            init_conn_callback(); //初始化 连接的回掉函数 http_handler_ ,static_res_hander 静态资源hander
            //定时器
            //epoller_->AddFd(Timer::getInstance()->getfd0(), EPOLLIN );
        }


        template<typename T>
        bool need_cache(T&& t) { //需要cache ?
            if constexpr(std::is_same_v<T, enable_cache<bool>>) {
                return t.value;
            }
            else {
                return false;
            }
        }


        void set_static_res_handler();

        void init_conn_callback() {
            set_static_res_handler();
            http_handler_check_ = [this](request& req, response& res,bool route_it=false) { //这里初始化了 http_handler_
                try {
                    bool success = http_router_.route(req.get_method(), req.get_url(), req, res,route_it); //调用route
                    if (!success) {
                        if (not_found_) {
                            not_found_(req, res); //调用 404
                        }
                        else
                            res.set_status_and_content(status_type::not_found, "404");
                            //res.set_status_and_content(status_type::bad_request, "the url is not right");
                        return false;
                    }
                    return true;
                }
                catch (const std::exception& ex) {
                    res.set_status_and_content(status_type::internal_server_error, ex.what()+std::string(" exception in business function"));
                    return false;
                }
                catch (...) {
                    res.set_status_and_content(status_type::internal_server_error, "unknown exception in business function");
                    return false;
                }                
            };
            http_handler_ = [this](request& req, response& res) { //这里初始化了 http_handler_
                res.set_headers(req.get_headers());
                this->http_handler_check_(req,res,true); //真正开始执行
            };


            ws_connection_check_ = [this](request& req,response & res)->bool{
                //LOG(DEBUG) << ("ws_connection_check_, url is %.*s",req.get_url().length(),req.get_url().data());
                return  ws_before_ap.invoke(std::string(req.get_url()), req, res);
            };
        }

        void run() { //启动服务器
            //init_dir(static_dir_); //初始化 路径 ?
            //init_dir(upload_dir_);
            Start(); //TODO call father class 进入循环
        }

        virtual void Start() override {
            int timeMS = -1;  /* epoll wait timeout == -1 无事件将阻塞 */
            if(!isClose_) { LOG(INFO) << ("========== Server start =========="); }
            while(!isClose_) {
                if(timeoutMS_ > 0) {
                    timeMS = timer_->GetNextTick(); //TODO 这个timeMS 有什么用
                }
                int eventCnt = epoller_->Wait(timeMS);
                for(int i = 0; i < eventCnt; i++) {
                    /* 处理事件 */
                    int fd = epoller_->GetEventFd(i);
                    uint32_t events = epoller_->GetEvents(i);
                    if(fd == listenFd_) {       //新的连接
                        DealListen_();
                    }
                    else if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) { //断开连接
                        LOG(INFO) << "break connection fd : " << fd;
                        assert(users_.count(fd) > 0);
                        CloseConn_(users_[fd].get());
                    }
                    else if(events & EPOLLIN) {         //有数据要读取
                        LOG(INFO) << "read connection fd : "<<fd;
                        assert(users_.count(fd) > 0);
                        DealRead_(users_[fd].get());
                    }
                    else if(events & EPOLLOUT) {        //数据写事件
                        LOG(INFO) << "write connection fd " << fd;
                        assert(users_.count(fd) > 0);
                        DealWrite_(users_[fd].get());
                    } else {
                        LOG(ERROR) << ("Unexpected event");
                    }
                }
            }
        }




// =========================== epoll_server 

    // 将fd 加入user_ 里
    void AddClient_(int fd, sockaddr_in addr,
            std::size_t max_req_size, long keep_alive_timeout,
            http_handler* handler, 
            http_handler_check * handler_check,
            ws_connection_check * ws_conn_check,
            std::string& static_dir,
            std::function<bool(request& req, response& res)>* upload_check
            ) {
        assert(fd > 0);
        //auto [iter,inserted] =  users_.try_emplace(fd,
                //std::make_shared<connection>( max_req_size,keep_alive_timeout, 
                    //handler,handler_check,
                    //static_dir,upload_check)
                //);
        //if(inserted )
            //iter->second->start();
        //在 users_里去查找 这个fd
        if( users_.find(fd) == users_.end() ){ //没有找到,创建一个新的连接
            users_.emplace( fd, 
                    std::make_shared<connection>( max_req_size,keep_alive_timeout, 
                        handler,handler_check,
                        ws_conn_check,
                        static_dir,upload_check)
                    );
            //users_[fd]->start(); // 设置conn_指向自己
        }
        users_[fd]->init(fd, addr); //初始化
        if( timeoutMS_ > 0) //加入 时间控制器里
            timer_->add(fd, timeoutMS_, [this, fd ,Conn = users_[fd].get()](){
                        LOG(DEBUG) << "Timer_ Close connection, fd = " << fd;
                        this->CloseConn_(Conn);
                    });
                    //std::bind(&http_server_::CloseConn_, this, users_[fd].get()));
            //timer_->add(fd, timeoutMS_, [this,fd](){
                    //LOG(INFO)("================ timer_ close");
                    //});
        epoller_->AddFd(fd, EPOLLIN | connEvent_); //加入epoller里的监听
        SetFdNonblock(fd); //设置无阻塞
        LOG(INFO) << "Client[" << users_[fd]->GetFd() << "] in!";
    }

//====================  epoll_server virtual
    //有新的client来了 把它加入
    virtual void DealListen_() override {
        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);
        do {
            //在listenFd_ 上创建监听
            int fd = accept(listenFd_, (struct sockaddr *)&addr, &len); 
            LOG(DEBUG) << "new connection ,fd : " << fd;
            if(fd <= 0) { return;}
            else if(HttpConn::userCount >= MAX_FD) {
                SendError_(fd, "Server busy!");
                LOG(WARNING) << "Clients is full!";
                return;
            }
            AddClient_(fd, addr,
                    max_req_buf_size_,    // max_req_size
                    keep_alive_timeout_,  // keep_alive_timeout
                    &http_handler_,       // http_handler& handler
                    &http_handler_check_, // 检查是否有对应的路由
                    &ws_connection_check_,// 检查websocket的连接是否允许
                    upload_dir_, //std::string& static_dir
                    upload_check_? &upload_check_ : nullptr // upload_check
                    );
        } while(listenEvent_ & EPOLLET);
    }

//====================  epoll_server virtual end


        //websocket 发起连接前的检查
        websocket_before_ap_mananger ws_before_ap;

        std::size_t max_req_buf_size_ = 3 * 1024 * 1024; //max request buffer size 3M
        long keep_alive_timeout_ = 60; //max request timeout 60s

        http_router http_router_;
        std::string static_dir_ = fs::absolute(__config__::static_dir).string(); //default
        std::string upload_dir_ = fs::absolute(__config__::upload_dir).string(); //default
        std::time_t static_res_cache_max_age_ = 0;

        bool enable_timeout_ = true;
        http_handler http_handler_ = nullptr; // 被加入到connection 里,主要作用是调用router
        http_handler_check http_handler_check_ = nullptr; //检查是否存在对应的路由
        ws_connection_check ws_connection_check_ = nullptr; //检查websocket 是否可以连接

        std::function<bool(request& req, response& res)> download_check_ = nullptr;
        std::vector<std::string> relate_paths_;
        std::function<bool(request& req, response& res)> upload_check_ = nullptr;

        std::function<void(request& req, response& res)> not_found_ = nullptr;
        std::function<void(request&, std::string&)> multipart_begin_ = nullptr;
        //std::function<bool(std::shared_ptr<connection<SocketType>>)> on_conn_ = nullptr; //作什么用的?
        

        size_t max_header_len_;
        check_header_cb check_headers_;

        bool need_response_time_ = false;

    };

    //template<typename T>
    //using http_server_proxy = http_server_<T, io_service_pool>;

    //using http_server = http_server_proxy<NonSSL>;
    //using http_ssl_server = http_server_proxy<SSL>;
 

}
