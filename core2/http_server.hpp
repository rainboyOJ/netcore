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
//#include "session_manager.hpp"
//#include "cookie.hpp"

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
        }

        void init_conn_callback() {
            //set_static_res_handler(); // TODO
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
        if( users_.find(fd) == users_.end() ){
            users_.emplace( fd, 
                    std::make_shared<connection>( max_req_size,keep_alive_timeout, 
                        handler,handler_check,
                        static_dir,upload_check)
                    );
        }
        users_[fd]->init(fd, addr); //users_ 是一个map,它会自动添加
        if(timeoutMS_ > 0) {
            //timer_->add(fd, timeoutMS_, std::bind(&WebServer::CloseConn_, this, &users_[fd]));
        }
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
        std::function<bool(request& req, response& res)> download_check_;
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
}
