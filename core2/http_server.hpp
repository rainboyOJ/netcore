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
            //init_conn_callback(); //初始化 连接的回掉函数 http_handler_ ,static_res_hander 静态资源hander
        }

    void AddClient_(int fd, sockaddr_in addr,
            std::size_t max_req_size, long keep_alive_timeout,
            http_handler& handler, std::string& static_dir,
            std::function<bool(request& req, response& res)>* upload_check
            ) {
    assert(fd > 0);
    if( users_.find(fd) == users_.end() ){
        users_.emplace( fd, 
                std::make_shared<connection>( max_req_size,keep_alive_timeout, handler,static_dir,upload_check)
                );
        //users_[fd].init(fd, addr); //users_ 是一个map,它会自动添加
    }
    else 
        //users_[fd].init(fd, addr); //users_ 是一个map,它会自动添加
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
            //AddClient_(fd, addr,
                    //max_req_buf_size_, //max_req_size
                    //keep_alive_timeout_, //keep_alive_timeout
                    //http_handler_, // http_handler& handler
                    //upload_dir_, //std::string& static_dir
                    //upload_check_? &upload_check_ : nullptr // upload_check
                    //);
                //io_service_pool_.get_io_service(), ssl_conf_, max_req_buf_size_, keep_alive_timeout_, http_handler_, upload_dir_,
                //upload_check_?&upload_check_ : nullptr
            //);
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
        http_handler http_handler_ = nullptr;
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
