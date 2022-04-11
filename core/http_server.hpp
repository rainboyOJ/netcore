#pragma once
#include <string>
#include <vector>
#include <string_view>

#include "define.h"
#include "heaptimer.h"

#include "epoll_server.hpp" 
#include "utils.hpp" 
#include "connection.hpp"
#include "ws_before_ap.h"
#include "http_router.hpp"
#include "http_cache.hpp"
#include "timer.hpp" //定时器
#include "hs_utils.h"

#ifdef __USE_SESSION__ //暂时没有实现这个功能
#include "session_manager.hpp"
#include "cookie.hpp"
#endif


namespace rojcpp {
    
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
                //,__config__::dbName//"rojcpp" //const char *dbName,
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
            epoller_->AddFd(Timer::getInstance()->getfd0(), EPOLLIN );
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

        /**
         * @desc 注册 websocket 连接检查函数
         */
        void regist_ws_conn_check(
                std::string_view url_name,
                websocket_before_ap_mananger::AP_Type&& ap
                ){
            ws_before_ap.regist(url_name, std::move(ap));
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
                LOG_DEBUG("ws_connection_check_, url is %.*s",req.get_url().length(),req.get_url().data());
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
            if(!isClose_) { LOG_INFO("========== Server start =========="); }
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
                    // 定时事件
                    else if ((fd == Timer::getInstance()->getfd0()) && (events & EPOLLIN))
                    {
                        //bool flag = dealwithsignal(timeout, stop_server);

                        char signals[1024];
                        int ret = recv(Timer::getInstance()->getfd0(), signals, sizeof(signals), 0);
                        threadpool_->AddTask( std::bind(&http_server_::deal_sigal , this,int(signals[0])) );
                        alarm(__config__::alarm_time); //alarm_time_ 检查一次
                        //epoller_->ModFd(Timer::get->GetFd(), connEvent_ | EPOLLIN);
                        //if (false == flag)
                            //LOG_ERROR("%s", "dealclientdata failure");
                    }
                    else if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) { //断开连接
                        LOG_INFO("break connection fd %d",fd);
                        assert(users_.count(fd) > 0);
                        CloseConn_(users_[fd].get());
                    }
                    else if(events & EPOLLIN) {         //有数据要读取
                        LOG_INFO("read connection fd %d",fd);
                        assert(users_.count(fd) > 0);
                        DealRead_(users_[fd].get());
                    }
                    else if(events & EPOLLOUT) {        //数据写事件
                        LOG_INFO("write connection fd %d",fd);
                        assert(users_.count(fd) > 0);
                        DealWrite_(users_[fd].get());
                    } else {
                        LOG_ERROR("Unexpected event");
                    }
                }
            }
        }

        //处理定时任务
        void deal_timer(){
            //connection 的超时
            //session_manager的超时
        }

        //信号处理
        void deal_sigal(int sig){
            //LOG_INFO("deal_sigal signal = %d\n",sig);
            if( sig == SIGALRM) { // 
                LOG_DEBUG("deal_sigal SIGALRM");
                LOG_DEBUG("check session_manager expire");
                session_manager::get().check_expire(); //检查session
            }
        }

        /**
         * desc: 核心函数,添加一个 http url的处理函数
         * 作用: 将 function 处理函数 注册到 http_router_ 里
         * TODO 为什么要 检查cache,cache 有什么用?
         */
        template<http_method... Is, typename Function, typename... AP>
        void set_http_handler(std::string_view name, Function&& f, AP&&... ap) {
            //只要AP里面有一个是 enable_cache<bool>类型
            if constexpr(has_type<enable_cache<bool>, std::tuple<std::decay_t<AP>...>>::value) {//for cache
                bool b = false;
                ((!b&&(b = need_cache(std::forward<AP>(ap)))),...); //折叠表达式
                if (!b) {
                    http_cache::get().add_skip(name); //TODO 核心是我不懂 http_cache 的作用
                }else{
                    http_cache::get().add_single_cache(name);
                }
                auto tp = filter<enable_cache<bool>>(std::forward<AP>(ap)...);
                auto lm = [this, name, f = std::move(f)](auto... ap) {
                    http_router_.register_handler<Is...>(name, std::move(f), std::move(ap)...);
                };
                std::apply(lm, std::move(tp));
            }
            else {
                http_router_.register_handler<Is...>(name, std::forward<Function>(f), std::forward<AP>(ap)...);
            }
        }

        template<http_method... Is, typename Function, typename... AP>
        void set_http_regex_handler(std::regex & name,Function&& f,const AP&&... ap){
            http_router_.register_handler_for_regex<Is...>(name, std::forward<Function>(f), std::forward<AP>(ap)...);
        }

        //设置 静态文件的目录
        void set_static_dir(std::string path) { 
            set_file_dir(std::move(path), static_dir_);
        }

        //设置上传文件的目录
        void set_upload_dir(std::string path) {
            set_file_dir(std::move(path), upload_dir_);
        }

        void set_file_dir(std::string&& path, std::string& dir) {
            
            //default: current path + "www"/"upload"
            //"": current path
            //"./temp", "temp" : current path + temp
            //"/temp" : linux path; "C:/temp" : windows path
            
            if (path.empty()) {
                dir = fs::current_path().string();
                return;
            }

            if (path[0] == '/' || (path.length() >= 2 && path[1] == ':')) {
                dir = std::move(path);
            }
            else {
                dir = fs::absolute(path).string();
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
                        LOG_DEBUG("Timer_ Close connection, fd = %d",fd);
                        this->CloseConn_(Conn);
                    });
                    //std::bind(&http_server_::CloseConn_, this, users_[fd].get()));
            //timer_->add(fd, timeoutMS_, [this,fd](){
                    //LOG_INFO("================ timer_ close");
                    //});
        epoller_->AddFd(fd, EPOLLIN | connEvent_); //加入epoller里的监听
        SetFdNonblock(fd); //设置无阻塞
        LOG_INFO("Client[%d] in!", users_[fd]->GetFd());
    }

//====================  epoll_server virtual
    //有新的client来了 把它加入
    virtual void DealListen_() override {
        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);
        do {
            //在listenFd_ 上创建监听
            int fd = accept(listenFd_, (struct sockaddr *)&addr, &len); 
            LOG_DEBUG("new connection ,fd is %d",fd);
            if(fd <= 0) { return;}
            else if(HttpConn::userCount >= MAX_FD) {
                SendError_(fd, "Server busy!");
                LOG_WARN("Clients is full!");
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
