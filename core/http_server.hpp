#pragma once
#include <string>
#include <vector>
#include <string_view>

#include "define.h"

#include "epoll_server.hpp" 
#include "utils.hpp" 
#include "connection.hpp"
#include "http_router.hpp"
////#include "router.hpp"
//#include "function_traits.hpp"
//#include "url_encode_decode.hpp"
#include "http_cache.hpp"
#include "timer.hpp" //定时器
#include "heaptimer.hpp"

#ifdef __USE_SESSION__
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
        http_server_() = default;

        void init(
        int port, int trigMode, int timeoutMS, bool OptLinger, 
        int sqlPort, const char* sqlUser, const  char* sqlPwd, 
        const char* dbName, int connPoolNum, int threadNum,
        bool openLog, int logLevel, int logQueSize
                )
        {
            __init__(port, trigMode, timeoutMS, OptLinger, sqlPort, sqlUser, sqlPwd, dbName, connPoolNum, threadNum, openLog, logLevel, logQueSize);
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

        std::string get_send_data(request& req, const size_t len) ;
        void write_chunked_header(request& req, std::shared_ptr<std::ifstream> in, std::string_view mime);
        void write_chunked_body(request& req) ;
        void write_ranges_header(request& req, std::string_view mime, std::string filename, std::string file_size) ;
        void write_ranges_data(request& req) ;
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
                        LOG_INFO("new connection ,fd is %d",fd);
                        DealListen_();
                    }
                    else if ((fd == Timer::getInstance()->getfd0()) && (events & EPOLLIN))
                    {
                        //bool flag = dealwithsignal(timeout, stop_server);

                        char signals[1024];
                        int ret = recv(Timer::getInstance()->getfd0(), signals, sizeof(signals), 0);
                        threadpool_->AddTask(
                                std::bind(&http_server_::deal_sigal , this,int(signals[0]))
                                );
                        alarm(5000); //5000s 检查一次
                        //epoller_->ModFd(Timer::get->GetFd(), connEvent_ | EPOLLIN);
                        //if (false == flag)
                            //LOG_ERROR("%s", "dealclientdata failure");
                    }
                    else if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) { //断开连接
                        LOG_INFO("break connection fd %d",fd);
                        assert(users_.count(fd) > 0);
                        CloseConn_(users_[fd].get());
                    }
                    else if(events & EPOLLIN) { //有数据要读取
                        LOG_INFO("read connection fd %d",fd);
                        assert(users_.count(fd) > 0);
                        DealRead_(users_[fd].get());
                    }
                    else if(events & EPOLLOUT) {
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

        void deal_sigal(int sig){
            //LOG_INFO("deal_sigal signal = %d\n",sig);
            if( sig == SIGALRM) { // 
                //LOG_INFO("deal_sigal SIGALRM");
                //session_manager::get().check_expire(); //检查session
            }
        }

        //set http handlers
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

        void set_static_dir(std::string path) { 
            set_file_dir(std::move(path), static_dir_);
        }

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

    void AddClient_(int fd, sockaddr_in addr,
            std::size_t max_req_size, long keep_alive_timeout,
            http_handler* handler, 
            http_handler_check * handler_check,
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
        if( users_.find(fd) == users_.end() ){
            users_.emplace( fd, 
                    std::make_shared<connection>( max_req_size,keep_alive_timeout, 
                        handler,handler_check,
                        static_dir,upload_check)
                    );
            //users_[fd]->start(); // 设置conn_指向自己
        }
        users_[fd]->init(fd, addr); //users_ 是一个map,它会自动添加
        if( timeoutMS_ > 0) //加入heap里
            timer_->add(fd, timeoutMS_, std::bind(&http_server_::CloseConn_, this, users_[fd].get()));
            //timer_->add(fd, timeoutMS_, [this,fd](){
                    //LOG_INFO("================ timer_ close");
                    //});
        epoller_->AddFd(fd, EPOLLIN | connEvent_);
        SetFdNonblock(fd);
        LOG_INFO("Client[%d] in!", users_[fd]->GetFd());
    }

//====================  epoll_server virtual
    //有新的client来了 把它加入
    virtual void DealListen_() override {
        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);
        do {
            int fd = accept(listenFd_, (struct sockaddr *)&addr, &len);
            if(fd <= 0) { return;}
            else if(HttpConn::userCount >= MAX_FD) {
                SendError_(fd, "Server busy!");
                LOG_WARN("Clients is full!");
                return;
            }
            AddClient_(fd, addr,
                    max_req_buf_size_, //max_req_size
                    keep_alive_timeout_, //keep_alive_timeout
                    &http_handler_, // http_handler& handler
                    &http_handler_check_, //检查是否有对应的路由
                    upload_dir_, //std::string& static_dir
                    upload_check_? &upload_check_ : nullptr // upload_check
                    );
        } while(listenEvent_ & EPOLLET);
    }

//====================  epoll_server virtual end


        std::size_t max_req_buf_size_ = 3 * 1024 * 1024; //max request buffer size 3M
        long keep_alive_timeout_ = 60; //max request timeout 60s

        http_router http_router_;
        std::string static_dir_ = fs::absolute("www").string(); //default
        std::string upload_dir_ = fs::absolute("www").string(); //default
        std::time_t static_res_cache_max_age_ = 0;

        bool enable_timeout_ = true;
        http_handler http_handler_ = nullptr; // 被加入到connection 里,主要作用是调用router
        http_handler_check http_handler_check_ = nullptr; //检查是否存在对应的路由
        std::function<bool(request& req, response& res)> download_check_ = nullptr;
        std::vector<std::string> relate_paths_;
        std::function<bool(request& req, response& res)> upload_check_ = nullptr;

        std::function<void(request& req, response& res)> not_found_ = nullptr;
        std::function<void(request&, std::string&)> multipart_begin_ = nullptr;
        //std::function<bool(std::shared_ptr<connection<SocketType>>)> on_conn_ = nullptr; //作什么用的?

        size_t max_header_len_;
        check_header_cb check_headers_;

        transfer_type transfer_type_ = transfer_type::CHUNKED;
        bool need_response_time_ = false;


    };

    //template<typename T>
    //using http_server_proxy = http_server_<T, io_service_pool>;

    //using http_server = http_server_proxy<NonSSL>;
    //using http_ssl_server = http_server_proxy<SSL>;
 
        //静态资源hander
        void http_server_::set_static_res_handler()
        {
            set_http_handler<POST,GET>(STATIC_RESOURCE, [this](request& req, response& res){
                if (download_check_) { // 如果有
                    bool r = download_check_(req, res);
                    if (!r) {
                        res.set_status_and_content(status_type::bad_request);
                        return;
                    }                        
                }

                auto state = req.get_state(); // TODO 这个state的作用是什么 ?
                switch (state) {
                    case rojcpp::data_proc_state::data_begin:
                    {
                        std::string relative_file_name = req.get_relative_filename();
                        std::string fullpath = static_dir_ + relative_file_name;

                        auto mime = req.get_mime(relative_file_name);
                        auto in = std::make_shared<std::ifstream>(fullpath, std::ios_base::binary);
                        if (!in->is_open()) {
                            if (not_found_) {
                                not_found_(req, res);
                                return;
                            }
                            res.set_status_and_content(status_type::not_found,"");
                            return;
                        }
                        auto start = req.get_header_value("cinatra_start_pos"); //设置读取的文件的位置
                        if (!start.empty()) {
                            std::string start_str(start);
                            int64_t start = (int64_t)atoll(start_str.data());
                            std::error_code code;
                            int64_t file_size = fs::file_size(fullpath, code);
                            if (start > 0 && !code && file_size >= start) {
                                in->seekg(start);
                            }
                        }
                        
                        req.get_conn()->set_tag(in); // tag变成in
                        
                        //if(is_small_file(in.get(),req)){
                        //    send_small_file(res, in.get(), mime);
                        //    return;
                        //}

                        if(transfer_type_== transfer_type::CHUNKED)
                            write_chunked_header(req, in, mime); //调用的是这个
                        else
                            write_ranges_header(req, mime, fs::path(relative_file_name).filename().string(), std::to_string(fs::file_size(fullpath)));
                    }
                        break;
                    case rojcpp::data_proc_state::data_continue:
                    {
                        if (transfer_type_ == transfer_type::CHUNKED)
                            write_chunked_body(req);
                        else
                            write_ranges_data(req);
                    }
                        break;
                    case rojcpp::data_proc_state::data_end:
                    {
                        auto conn = req.get_conn();
                        conn->clear_continue_workd(); // 结束
                    }
                        break;
                    case rojcpp::data_proc_state::data_error:
                    {
                        //network error
                    }
                        break;
                }
            },enable_cache{false});
        }


        void http_server_::write_chunked_header(request& req, std::shared_ptr<std::ifstream> in, std::string_view mime) {
            auto range_header = req.get_header_value("range");
            req.set_range_flag(!range_header.empty()); //分块发送
            req.set_range_start_pos(range_header);

            std::string res_content_header = std::string(mime.data(), mime.size()) + "; charset=utf8";
            res_content_header += std::string("\r\n") + std::string("Access-Control-Allow-origin: *");
            res_content_header += std::string("\r\n") + std::string("Accept-Ranges: bytes");
            if (static_res_cache_max_age_>0)
            {
                std::string max_age = std::string("max-age=") + std::to_string(static_res_cache_max_age_);
                res_content_header += std::string("\r\n") + std::string("Cache-Control: ") + max_age;
            }
            
            if(req.is_range())
            {
                std::int64_t file_pos  = req.get_range_start_pos();
                in->seekg(file_pos);
                auto end_str = std::to_string(req.get_request_static_file_size());
                res_content_header += std::string("\r\n") +std::string("Content-Range: bytes ")+std::to_string(file_pos)+std::string("-")+std::to_string(req.get_request_static_file_size()-1)+std::string("/")+end_str;
            }
            //高用了connection的 write_chunked_header
            req.get_conn()->write_chunked_header(std::string_view(res_content_header),req.is_range());
        }

        std::string http_server_::get_send_data(request& req, const size_t len) {
            auto conn = req.get_conn();
            auto in = std::any_cast<std::shared_ptr<std::ifstream>>(conn->get_tag());
            std::string str;
            str.resize(len);
            in->read(&str[0], len);
            size_t read_len = (size_t)in->gcount();
            if (read_len != len) {
                str.resize(read_len);
            }
            return str;
        }

        void http_server_::write_chunked_body(request& req) {
            const size_t len = 3 * 1024 * 1024;
            auto str = get_send_data(req, len); //得到发送的数据
            auto read_len = str.size();
            bool eof = (read_len == 0 || read_len != len);
            req.get_conn()->write_chunked_data(std::move(str), eof);
        }


        void http_server_::write_ranges_header(request& req, std::string_view mime, std::string filename, std::string file_size) {
            std::string header_str = "HTTP/1.1 200 OK\r\nAccess-Control-Allow-origin: *\r\nAccept-Ranges: bytes\r\n";
            header_str.append("Content-Disposition: attachment;filename=");
            header_str.append(std::move(filename)).append("\r\n");
            header_str.append("Connection: keep-alive\r\n");
            header_str.append("Content-Type: ").append(mime).append("\r\n");
            header_str.append("Content-Length: ");
            header_str.append(file_size).append("\r\n\r\n");
            req.get_conn()->write_ranges_header(std::move(header_str));
        }

        void http_server_::write_ranges_data(request& req) {
            const size_t len = 3 * 1024 * 1024;
            auto str = get_send_data(req, len);
            auto read_len = str.size();
            bool eof = (read_len == 0 || read_len != len);
            req.get_conn()->write_ranges_data(std::move(str), eof);
        }

}
