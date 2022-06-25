//创建一个Server,支持多线程

#pragma once

#include <iostream>

#include <thread>
#include "logWrapper.hpp"
#include "core.hpp"
#include "connection.hpp"
#include "http_router.hpp"
#include "ws_before_ap.hpp"
#include "hs_utils.hpp"

namespace netcore {


class http_server;

using http_handler        = std::function<void(request&, response&)>;
using http_handler_check  = std::function<bool(request&, response&,bool)>;
using ws_connection_check = std::function<bool(request&, response&)>;
using send_ok_handler     = std::function<void()>;
using send_failed_handler = std::function<void(const int&)>;
using upload_check_handler= std::function<bool(request& req, response& res)>;

template<typename Http_Server>
class Server {
    friend http_server;
    private:
        IoContext m_ctx;
        std::thread m_thread;
        Acceptor m_acceptor;
        std::string static_dir = "www";
        int m_id;
        Http_Server * m_hsp;


    public:
        Server()  = delete;
        Server(Http_Server * hsp,int id,Acceptor & acc)
            :m_hsp{hsp}, m_id(id)
        {
            m_acceptor.reset_io_context(m_ctx, acc);
        }

        Server(Http_Server * hsp,int id,NativeHandle epollfd,Acceptor & acc)
            :m_hsp{hsp}, m_id(id),m_ctx(epollfd)
        {
            m_acceptor.reset_io_context(m_ctx, acc);
        }

//================和 coroutines 配合


        Task listen() //启动
        {
            for(;;) {
                LOG(INFO) << "async_accept at thread : " << m_id;
                auto conn = co_await m_acceptor.async_accept();
                auto conn_sp = m_hsp->make_connection(std::move(conn));
                co_spawn(handle(conn_sp));
            }
        }
        //运行
        void Serve(){
            m_thread = std::thread([this]{
                    try {
                        LOG(INFO) << "Server start id : " <<m_id;
                        co_spawn(listen());
                        LOG(INFO) << "Server run at server id : " << m_id;
                        m_ctx.run();
                    }
                    catch(const std::exception &e){
                        //LOG(std::current_exception());
                        LOG(INFO) << "catch, error 1";
                        LOG(ERROR) << e.what();
                    }
                    catch(...){
                        LOG(INFO) << "catch, error 2";
                    }
                    m_hsp->stop_thread.store(true);
                });
        }

        //处理发生connection
        Task handle(std::shared_ptr<connection> conn_handle) 
        {
            try {
                bool for_1_end = false;
                for(; !for_1_end ;) { //无限循环,直接 need_write

                    bool break_for_flag = false;

                    //auto conn_handle = m_hsp->make_connection(conn->socket());

                    for(;;){

                        auto nbytes = co_await conn_handle->get_raw_conn()->async_read(conn_handle->req().buffer(),conn_handle->req().left_size(),100s);
                        // 进行相应的处理
                        if( nbytes == 0){
                            break_for_flag = true;
                            break ;
                        }
                        if( conn_handle->req().update_size(nbytes)) {
                            LOG(DEBUG) << "overflow MaxSize";
                            conn_handle->res().set_status_and_content(status_type::bad_request,"overflow MaxSize 3M");
                            break_for_flag = true;
                            break ;
                        }
                        LOG(DEBUG) << "async_read bytes size : " << nbytes;
                        // 读取的数据
                        //for(int i = 0 ;i <  conn_handle->req().current_size() ; i++) {
                        //    std::cout << conn_handle->req().data()[i] ;
                        //}
                        //std::cout  << std::endl;

                        auto pc_state = conn_handle->process();
                        for_1_end = pc_state == process_state::need_write;
                        if( pc_state == process_state::need_read)
                            continue;
                        else break;
                    };

                    if( break_for_flag ) co_return; // 直接退出

                    LOG(DEBUG) << "---process read end ,need to write";

                    //auto send_bufs = conn_handle->res().to_buffers();
                    auto & rep_str = conn_handle->res().response_str();
                    // 这里改成一次不发送完整,而是按128K一次的发送
                    auto nbytes = co_await conn_handle->get_raw_conn()->async_send(rep_str.data(),rep_str.length(),100s);

                    LOG(DEBUG) << "async _send end";

                }
            }
            catch(const netcore::SendError& e){
                LOG(INFO) << e.what();
                conn_handle->get_raw_conn()->clear_except();
            }
            catch(const netcore::RecvError& e){
                LOG(INFO) << e.what();
                conn_handle->get_raw_conn()->clear_except();
            }
            catch(const netcore::IoTimeOut& e){
                LOG(INFO) << e.what();
                conn_handle->get_raw_conn()->clear_except();
            }

        }
};
    
//============================================

class http_server {

    using Server_type = Server<http_server>;
private:
    std::size_t m_num; //线程的数量
    std::vector<Server_type> m_servers;
    unsigned int m_port;
    Acceptor m_acceptor;

public:

    std::atomic_bool stop_thread = false;

private:


    std::string static_dir_ = fs::absolute(__config__::static_dir).string(); //default
    std::string upload_dir_ = fs::absolute(__config__::upload_dir).string(); //default
    
    http_handler_check http_handler_check_ = nullptr; //检查是否存在对应的路由
    http_router http_router_;

    websocket_before_ap_mananger ws_before_ap;

    std::size_t max_req_buf_size_ = 3 * 1024 * 1024; //max request buffer size 3M
    long keep_alive_timeout_ = 60; //max request timeout 60s

    std::time_t static_res_cache_max_age_ = 0;

    bool enable_timeout_ = true;
    http_handler http_handler_ = nullptr; // 被加入到connection 里,主要作用是调用router
    ws_connection_check ws_connection_check_ = nullptr; //检查websocket 是否可以连接

    std::function<bool(request& req, response& res)> download_check_ = nullptr;
    std::vector<std::string> relate_paths_;
    std::function<bool(request& req, response& res)> upload_check_ = nullptr;

    std::function<void(request& req, response& res)> not_found_ = nullptr;
    std::function<void(request&, std::string&)> multipart_begin_ = nullptr;
    //std::function<bool(std::shared_ptr<connection<SocketType>>)> on_conn_ = nullptr; //作什么用的?

public:
    http_server() = delete;
    http_server(std::size_t num,unsigned int port) 
        :m_port(port),m_num(num),
        m_acceptor(Protocol::ip_v4(),Endpoint(Address::Any(),port))
    {
        init_conn_callback();
        //::accept(int __fd, struct sockaddr *__restrict __addr, socklen_t *__restrict __addr_len)
        m_servers.reserve(num);
        for(int i=0;i<num;++i){
            m_servers.emplace_back(this,i,m_acceptor);
        }
    }

    ~http_server() {
        for(int i=0;i<m_num;++i){
            if(m_servers[i].m_thread.joinable())
                m_servers[i].m_thread.join();
        }
    }
    void run() //运行
    {
        for(int i=0;i<m_num;++i){
            m_servers[i].Serve();
        }
    }

//================ 基础的方法
//
    auto make_connection(RawConnection::CONN_PTR conn) {
        auto conn_shr = std::make_shared<connection>(
                conn,
                3*1024*1024, //max_req_size 3MB
                10*60, // keep_alive_timeout
                &http_handler_,      //http_handler  的函数指针
                &http_handler_check_, // http_handler_check 的函数指针
                &ws_connection_check_ ,// ws_connection_check * ws_conn_check,
                static_dir_,    //静态资源目录
                upload_check_ ?  &upload_check_ : nullptr,//upload_check_handler * upload_check //上传查询
                conn->socket()
                );
        conn_shr->init(); //TODO 应该传入addr
        return conn_shr;
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

    void set_static_res_handler() {
        set_http_handler<POST,GET>(STATIC_RESOURCE, [this](request& req, response& res){
            if (download_check_) { // 如果有下载检查
                bool r = download_check_(req, res);
                if (!r) { //不成功
                    res.set_status_and_content(status_type::bad_request);
                    return;
                }                        
            }
            
            std::string relative_file_name = req.get_relative_filename();
            std::string fullpath = std::string(__config__::static_dir) + relative_file_name;
            hs_utils::process_download(fullpath, req, res);

        });
    }

        void init_conn_callback() {
            set_static_res_handler();
            http_handler_check_ = [this](request& req, response& res,bool route_it=false) { //这里初始化了 http_handler_
                LOG(DEBUG) << __PRETTY_FUNCTION__ ;
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

        /**
         * desc: 核心函数,添加一个 http url的处理函数
         * 作用: 将 function 处理函数 注册到 http_router_ 里
         * TODO 为什么要 检查cache,cache 有什么用?
         */
        template<http_method... Is, typename Function, typename... AP>
        void set_http_handler(std::string_view name, Function&& f, AP&&... ap) {
            http_router_.register_handler<Is...>(name, std::forward<Function>(f), std::forward<AP>(ap)...);
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

};


} // end namespace netcore

