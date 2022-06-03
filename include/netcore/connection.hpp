#pragma once
#include <vector>
#include <cassert>
#include <mutex>
#include <any>

#include <sys/types.h>
#include <sys/uio.h>     // readv/writev
#include <arpa/inet.h>   // sockaddr_in
#include <stdlib.h>      // atoi()
#include <errno.h>      

#include "define.h"

#include "request.hpp" //请求
#include "response.hpp" //响应
#include "websocket.hpp" //websocket
#include "http_cache.hpp"
#include "multipart_reader.hpp"
//#include "buffer.h"
#include "websocket_manager.h"

#include "core.hpp"

namespace netcore {

    using http_handler        = std::function<void(request&, response&)>;
    using http_handler_check  = std::function<bool(request&, response&,bool)>;
    using ws_connection_check = std::function<bool(request&, response&)>;
    using send_ok_handler     = std::function<void()>;
    using send_failed_handler = std::function<void(const int&)>;
    using upload_check_handler= std::function<bool(request& req, response& res)>;


    enum class process_state {
        need_read,
        need_write,
        need_ws
    };

    using SocketType = int;
    class connection : private noncopyable , public std::enable_shared_from_this<connection>
    {
    public:
        explicit connection(
                std::size_t max_req_size,
                long keep_alive_timeout,
                http_handler* handler,      //http_handler  的函数指针
                http_handler_check * handler_check, // http_handler_check 的函数指针
                ws_connection_check * ws_conn_check,
                std::string& static_dir,    //静态资源目录
                upload_check_handler * upload_check, //上传查询
                NativeSocket fd_
            )
            :
            MAX_REQ_SIZE_(max_req_size), KEEP_ALIVE_TIMEOUT_(keep_alive_timeout),
            http_handler_(handler), http_handler_check_(handler_check),
            ws_connection_check_(ws_conn_check),
            req_(res_), static_dir_(static_dir), upload_check_(upload_check),
            fd_{fd_}
        {
            init_multipart_parser(); //初始化 multipart 解析器
        }


        int GetFd() const { return fd_;}

        auto & req() {
            return req_;
        }
        auto & res() {
            return res_;
        }

        //本地地址
        std::string local_address() const { 
            if (has_closed_) {
                return "";
            }
            //std::stringstream ss;
            return "unknown"; // TODO fix
        }

        const std::string& static_dir() {
            return static_dir_;
        }

        //在发生错误的时间返回错误信息
        void on_error(status_type status, std::string&& reason) {
            keep_alive_ = false;
            req_.set_state(data_proc_state::data_error);
            response_back(status, std::move(reason));
        }

        void on_close() {
            keep_alive_ = false;
            req_.set_state(data_proc_state::data_error);
            //connection_close();
            Close();
        }

        process_state process() {
            if (req_.at_capacity()) {  //超过最大容量
                response_back(status_type::bad_request, "The request is too long, limitation is 3M"); //TODO 3M
                return process_state::need_write;
            }

            if( !has_continue_workd() )
                return handle_read(last_transfer_);
            else
                return (this->*continue_work_)();
        }

        //============ web socket ============
        //template<typename... Fs>
        //void send_ws_string(std::string msg, Fs&&... fs) {
            //send_ws_msg(std::move(msg), opcode::text, std::forward<Fs>(fs)...);
        //}
        bool is_ws_socket() const {
            return is_upgrade_;
        }
        template<typename... Fs>
        void send_ws_string(std::string&& msg, Fs&&... fs) {
            send_ws_msg(std::move(msg), opcode::text, std::forward<Fs>(fs)...);
        }

        template<typename... Fs>
        void send_ws_binary(std::string msg, Fs&&... fs) {
            send_ws_msg(std::move(msg), opcode::binary, std::forward<Fs>(fs)...);
        }

        template<typename... Fs>
        void send_ws_msg(std::string msg, opcode op = opcode::text, Fs&&... fs) {
            constexpr const size_t size = sizeof...(Fs);
            static_assert(size == 0 || size == 2); //必须这两个之一
            if constexpr(size == 2) {
                set_callback(std::forward<Fs>(fs)...);
            }

            auto header = websocket::format_header(msg.length(), op);
            send_msg(std::move(header), std::move(msg));
        }
        //============ web socket end ============

        //============ file send ============
        void write_chunked_header(std::string_view mime,bool is_range=false) {
            req_.set_http_type(content_type::chunked); //设置内容为 chunked
            if(!is_range){
                chunked_header_ = http_chunk_header + "Content-Type: " + std::string(mime.data(), mime.length()) + "\r\n\r\n";
            }else{
                chunked_header_ = http_range_chunk_header + "Content-Type: " + std::string(mime.data(), mime.length()) + "\r\n\r\n";
            }
            res_.response_str() = std::move(chunked_header_);
            req_.set_state(data_proc_state::data_continue); //设置data_continue 会读取req的
            //设置下一个任务
            continue_work_ = &connection::continue_write_then_route; //
        }

        //写一个 ranges 头
        void write_ranges_header(std::string&& header_str) {
            res_.response_str() = std::move(header_str);
            req_.set_state(data_proc_state::data_continue); //设置data_continue 会读取req的
            //设置下一个任务
            continue_work_ = &connection::continue_write_then_route; //
        }

        //写数据 chunk_data
        void write_chunked_data(std::string&& buf, bool eof) {
            res_.response_str() = res_.to_chunked_buffers(buf.data(),buf.length() , eof);
            if (eof) {
                req_.set_state(data_proc_state::data_end); //data_end 应该如何做呢
            }
            else {
                req_.set_state(data_proc_state::data_continue);
            }
        }

        // TODO 什么是 ranges_data 文件的一个区间
        void write_ranges_data(std::string&& buf, bool eof) {
            res_.response_str() = std::move(buf);
            if (eof) {
                req_.set_state(data_proc_state::data_end);
            }
            else {
                req_.set_state(data_proc_state::data_continue);
            }
        }

        //设置处理函数
        void set_multipart_begin(std::function<void(request&, std::string&)> begin) {
            multipart_begin_ = std::move(begin);
        }

        // ?
        void set_validate(size_t max_header_len, check_header_cb check_headers) {
            req_.set_validate(max_header_len, std::move(check_headers));
        }

        // ?
        void enable_response_time(bool enable) {
            res_.enable_response_time(enable);
        }

        bool has_close() const {
            return has_closed_;
        }

        response& get_res() {
            return res_;
        }

        ~connection() {
            Close();
        }

    private:
        void reset() {
            last_transfer_ = 0;
            req_.reset();
            res_.reset();
        }


        //core 处理读取的数据
        process_state handle_read(std::size_t bytes_transferred) { 
            //auto last_len = req_.current_size();
            //last_transfer_ = last_len;
            //LOG_DEBUG("this pointer is %llx",static_cast<void *>(this));
            //int ret = req_.parse_header_expand_last_len(last_transfer_); //解析头
            int ret = req_.parse_header(0); //解析头
#ifdef __NETCORE__DEBUG__
            LOG(DEBUG) << "METHOD: " << req_.get_method();
            LOG(DEBUG) << "URL: " << req_.get_url();

            //printf("parse_header  result begin ========================\n");
            LOG(DEBUG) << "====== The request headers were : ======";
            auto [_headers_,_num_headers_] = req_.get_headers();
            for(int i = 0 ;i < _num_headers_;i++){
                LOG(INFO) << std::string_view(_headers_[i].name,_headers_[i].name_len) 
                    << ": "
                    << std::string_view(_headers_[i].value,_headers_[i].value_len);
            }
            //printf("parse_header  result end ========================\n");
#endif


            if (ret == parse_status::has_error) { 
                response_back(status_type::bad_request);
                return process_state::need_write; // 进入写入
            }
            if( !peek_route() ) { //找不到对应的路由
                LOG(DEBUG) << "peek_route Failed" ;
                //TODO 404
                //response_back(status_type::404notfoudn); // 改写res为404响应
                return process_state::need_write;
            }

            check_keep_alive();
            if (ret == parse_status::not_complete) {
                return process_state::need_read;
            }
            else {
                return handle_request(bytes_transferred);
            }
        }

        //核心 
        // 处理和各种类型的body
        // 如果没有 body 直接 处理头
        process_state handle_request(std::size_t bytes_transferred) { //处理请求
            if (req_.has_body()) {
                // TODO check call back is having this route
                // {
                //  res_.set_respone(bad:restures."404")
                //  return TO_EPOLL_WRITE;
                // }
                auto type = get_content_type();
                req_.set_http_type(type);
                switch (type) {
                    case netcore::content_type::string:
                    case netcore::content_type::json:
                    case netcore::content_type::unknown:
                        return handle_string_body(bytes_transferred);
                    case netcore::content_type::multipart:
                        return handle_multipart();
                    case netcore::content_type::octet_stream:
                        //handle_octet_stream(bytes_transferred);
                        //response_back(status_type status)
                        // 补全
                        return process_state::need_write;
                    case netcore::content_type::urlencoded:
                        handle_form_urlencoded(bytes_transferred);
                        break;
                    case netcore::content_type::chunked:
                        handle_chunked(bytes_transferred);
                        break;
                    case netcore::content_type::websocket:
                        //TODO 对ws 应该如何处理 
                        //是不是应该返回错误
                        break;
                }
                return process_state::need_write;
            }
            else {
                return handle_header_request();
            }
        }

        //得到body类型
        content_type get_content_type() {
            if (req_.is_chunked())
                return content_type::chunked;

            auto content_type = req_.get_header_value("content-type");
            if (!content_type.empty()) {
                if (content_type.find("application/x-www-form-urlencoded") != std::string_view::npos) {
                    return content_type::urlencoded;
                }
                //Content-Type:	multipart/form-data; boundary=----WebKitFormBoundaryX4zzs3sYStDhVBUX
                else if (content_type.find("multipart/form-data") != std::string_view::npos) {
                    auto size = content_type.find("=");
                    auto bd = content_type.substr(size + 1, content_type.length() - size);
                    if (bd[0] == '"'&& bd[bd.length()-1] == '"') {
                        bd = bd.substr(1, bd.length() - 2);
                    }
                    std::string boundary(bd.data(), bd.length());
                    //根据协议 在body 里的分界开头多个两个--
                    multipart_parser_.set_boundary("\r\n--" + std::move(boundary));
                    return content_type::multipart;
                }
                else if (content_type.find("application/octet-stream") != std::string_view::npos) {
                    return content_type::octet_stream;
                }
                else if (content_type.find("application/json") != std::string_view::npos) {
                    return content_type::json;
                }
                else {
                    return content_type::string;
                }
            }

            return content_type::unknown;
        }

        /****************** begin handle http body data *****************/
        process_state handle_string_body(std::size_t bytes_transferred) {
            //defalt add limitation for string_body and else. you can remove the limitation for very big string.
            if (req_.at_capacity()) {
                response_back(status_type::bad_request, "The request is too long, limitation is 3M");
                return process_state::need_write;
            }

            if (req_.has_recieved_all()) {
                handle_body();
                return process_state::need_write;
            }
            else {
                req_.expand_size();
                assert(req_.current_size() >= req_.header_len());
                size_t part_size = req_.current_size() - req_.header_len();
                if (part_size == -1) {
                    response_back(status_type::internal_server_error);
                    return process_state::need_write;
                }
                req_.reduce_left_body_size(part_size);
                return process_state::need_read;
            }
        }

        //-------------form urlencoded----------------//
        //TODO: here later will refactor the duplicate code
        void handle_form_urlencoded(size_t bytes_transferred) {
            if (req_.at_capacity()) {
                response_back(status_type::bad_request, "The request is too long, limitation is 3M");
                return;
            }

            if (req_.has_recieved_all()) {
                handle_url_urlencoded_body();
            }
            else {
                req_.expand_size();
                size_t part_size = bytes_transferred - req_.header_len();
                req_.reduce_left_body_size(part_size);
                //req_.fit_size();
                //do_read_form_urlencoded();
                //TODO change to return process_state::need_read
            }
        }

        void handle_url_urlencoded_body() {
            bool success = req_.parse_form_urlencoded();

            if (!success) {
                response_back(status_type::bad_request, "form urlencoded error");
                return;
            }
            if (req_.body_len() > 0 && !req_.check_request()) {
                response_back(status_type::bad_request, "request check error");
                return;
            }

            call_back();
            //if (!res_.need_delay()) //这里应该不需要delay 所以我注释了
                //do_write();
        }

        //-------------form urlencoded----------------//

        void call_back() {
            assert(http_handler_);
            (*http_handler_)(req_, res_);
        }

        bool peek_route(){
            assert(http_handler_check_);
            return (*http_handler_check_)(req_,res_,false);
        }

        void call_back_data() {
            req_.set_state(data_proc_state::data_continue);
            call_back();
            req_.set_part_data({});
        }
        //-------------multipart----------------------// /上传文件
        void init_multipart_parser() { //初始化
                //part的开始 
                //on_part_begin 其时在一个part在头解析结束时候的动作
                multipart_parser_.on_part_begin = [this](const multipart_headers & headers) {
                    //LOG_DEBUG("=============== on_part_begin ");
                    req_.set_multipart_headers(headers); //加头 TODO 加到req_的哪里
                    auto filename = req_.get_multipart_field_name("filename");
                    is_multi_part_file_ = req_.is_multipart_file(); //是否是文件
                    if (filename.empty()&& is_multi_part_file_) { //是文件，但是没有文件名
                        req_.set_state(data_proc_state::data_error);
                        res_.set_status_and_content(status_type::bad_request, "mutipart error");
                        return;
                    }                        
                    if(is_multi_part_file_)
                    {
                        LOG(DEBUG) << "is_multi_part_file_";
                        auto ext = get_extension(filename);
                        try {
                            auto tp = std::chrono::high_resolution_clock::now(); //弄时间 作为名字
                            auto nano = tp.time_since_epoch().count();
                            std::string name = static_dir_ + "/" + std::to_string(nano)
                                + std::string(ext.data(), ext.length())+"_ing";
                            if (multipart_begin_) { //TODO 如果有这个函数 运行它
                                multipart_begin_(req_, name); 
                                name = static_dir_ + "/" + name;
                            }
                            
                            LOG(DEBUG) << "open_upload_file : " << name;
                            req_.open_upload_file(name); //打开这个文件
                        }
                        catch (const std::exception& ex) {
                            req_.set_state(data_proc_state::data_error);
                            res_.set_status_and_content(status_type::internal_server_error, ex.what());
                            return;
                        }                        
                    }else{
                        auto key = req_.get_multipart_field_name("name");
                        req_.save_multipart_key_value(std::string(key.data(),key.size()),"");
                    }
                };
                //part的的数据
                multipart_parser_.on_part_data = [this](const char* buf, size_t size) {
                    //LOG_DEBUG("=============== on_part_data ");
                    if (req_.get_state() == data_proc_state::data_error) {
                        return;
                    }
                    if(is_multi_part_file_){ //写文件
                        req_.write_upload_data(buf, size);
                    }else{
                        auto key = req_.get_multipart_field_name("name");
                        req_.update_multipart_value(std::move(key), buf, size);
                    }
                };
                //part的的结束
                multipart_parser_.on_part_end = [this] { //这里是把文件保存下来
                    //LOG_INFO("=============== on_part_end ");
                    if (req_.get_state() == data_proc_state::data_error)
                        return;
                    if(is_multi_part_file_)
                    {
                        req_.close_upload_file(); //关闭最后一个文件
                        auto pfile = req_.get_file();
                        if (pfile) {
                            auto old_name = pfile->get_file_path();
                            auto pos = old_name.rfind("_ing");
                            //LOG_INFO("old_name : %s",old_name.c_str());
                            if (pos != std::string::npos) {
                                pfile->rename_file(old_name.substr(0, old_name.length() - 4)); //把_ing 去掉
                            }                            
                        }
                    }
                };
                //整个结束
                multipart_parser_.on_end = [this] {
                    if (req_.get_state() == data_proc_state::data_error)
                        return;
                    req_.handle_multipart_key_value(); // TODO 做什么？
                    //call_back(); 
                };        
        }

        /**
         * @return 返回值是否有错误
         */
        //解析 multipart
        bool parse_multipart(size_t size, std::size_t length) {
            //LOG_DEBUG("Run parse_multipart size %d,length %d",size,length);
            if (length == 0)
                return false;

            req_.set_part_data(std::string_view(req_.buffer(size), length));
            std::string_view multipart_body = req_.get_part_data();
            size_t bufsize = multipart_body.length();

            size_t fed = 0;
            do {
                size_t ret = multipart_parser_.feed(multipart_body.data() + fed, multipart_body.length() - fed);
                fed += ret;
            } while (fed < bufsize && !multipart_parser_.stopped());

            if (multipart_parser_.has_error()) {
                LOG(ERROR) <<  "multipart parser error message :" << multipart_parser_.get_error_message();
                req_.set_state(data_proc_state::data_error);
                return true;
            }

            req_.reduce_left_body_size(length);
            return false;
        }

        //处理multipart
        process_state handle_multipart() {
            //LOG_DEBUG("=====================handle_multipart=====================");
            if (upload_check_) {
                bool r = (*upload_check_)(req_, res_);
                if (!r) {
                    Close();
                    response_back(status_type::bad_request, "upload_check_ error at line "+ std::to_string(__LINE__));
                    return process_state::need_write;
                }                    
            }

            //LOG_DEBUG("req_ current_size %lld",req_.current_size());
            //LOG_DEBUG("req_ header_len %lld",req_.header_len());
            bool has_error = parse_multipart(req_.header_len(), req_.current_size() - req_.header_len());
            //LOG_DEBUG("has_error %d",has_error);
            if (has_error) {
                response_back(status_type::bad_request, "mutipart error");
                return process_state::need_write;
            }

            //LOG_DEBUG("req_.has_recieved_all_part() %d",req_.has_recieved_all_part());
            if (req_.has_recieved_all_part()) { //接收完全部的数据,结束
                call_back();
                return process_state::need_write;
            }
            else {
                req_.set_current_size(0);
                continue_work_ = &connection::do_read_multipart;
                return process_state::need_read;
            }
        }

        //读multipart
        process_state do_read_multipart() {
            //LOG_DEBUG("Run Function do_read_multipart");
            req_.fit_size();
            //auto self = this->shared_from_this();
            //if (ec) {
                //req_.set_state(data_proc_state::data_error);
                //call_back();
                //response_back(status_type::bad_request, "mutipart error");
                //return;
            //}

            bool has_error = parse_multipart(0, last_transfer_);
            //LOG_DEBUG("has_error %d",has_error);

            if (has_error) { //parse error
                keep_alive_ = false;
                response_back(status_type::bad_request, "mutipart error");
                clear_continue_workd();
                return process_state::need_write;
            }

            if (req_.body_finished()) {
                call_back();
                clear_continue_workd();
                return process_state::need_write;
            }

            req_.set_current_size(0);
            continue_work_ = &connection::do_read_multipart;
            return process_state::need_read;
        }

        //-------------multipart----------------------//

        //处理 只有 headers 的情况
        process_state handle_header_request() {
            if (is_upgrade_) { //websocket
                //TODO 增加一个检查是否可以进行websock连接的函数
                // 注意!! 这里使用了 route的_ap
                // 1. 是否有 before ap ,没有不可以ws
                // 2. 执行before ap
                //
                if( ws_connection_check_ != nullptr
                    && (*ws_connection_check_)(req_,res_) == false ) {
                    is_upgrade_ = false;
                    keep_alive_ = false;
                    res_.set_status_and_content(status_type::forbidden,"not allowed");
                    return process_state::need_write;
                }
                req_.set_http_type(content_type::websocket);
                //timer_.cancel();
                ws_.upgrade_to_websocket(req_, res_);   //生成 res_里的返回内容
                response_handshake();           //写内容
                return process_state::need_read;
            }

            bool r = handle_gzip();
            if (!r) {
                response_back(status_type::bad_request, "gzip uncompress error");
                return process_state::need_write;
            }

            call_back(); // 调用 http_router_ 其时就是route

            //默认就是 写
            //if (req_.get_content_type() == content_type::chunked) //这里
                //return TO_EPOLL_WRITE;

            if (req_.get_state() == data_proc_state::data_error) {
                return process_state::need_write;
            }

            return process_state::need_write;

        }

        //-------------web socket----------------//
        //@desc 返回握手信息,其实就是直接写数据
        void response_handshake()
        {
            std::vector<std::string> buffers = res_.to_buffers();
            //LOG_DEBUG("response_handshake : %d",buffers.empty());
            std::string buf_to_send;
            if (buffers.empty()) {
                Close(); // TODO 这里要改 我们不主动的关闭自己
                return;
            }
            // 直接写
            for (auto& e : buffers) {
                //LOG_DEBUG("%s,%d",e.c_str(),e.length());
                buf_to_send.append(std::move(e));
            }
            //LOG_DEBUG("%s",buf_to_send.c_str());
            direct_write(buf_to_send.c_str(), buf_to_send.length());
            //set_continue_workd
            //continue_work_ = &connection::ws_response_handshake_continue_work;

            req_.set_state(data_proc_state::data_begin); // alias ws_open
            call_back();
            req_.call_event(req_.get_state());
            req_.set_current_size(0); //清空
            //do_read_websocket_head(SHORT_HEADER);
            continue_work_ = &connection::handle_ws_data; //以后读取的数据会进入 handle_ws_data
            //return TO_EPOLL_READ; //转入读取
        }

        // 直接写数据,而不是写res 里的内容
        ssize_t direct_write(const char * data ,std::size_t len /*, int* saveErrno */) {
            //LOG_DEBUG("write data %s \n",res_.response_str().c_str());
            Writen(data,len); //每一次写在rep_str_里
            //last_transfer_ = 0 ;
            return len;
        }

        //返回写入的长度，<=0 是错误
        int Writen(const char *buffer,const size_t n)
        {
            int nLeft,idx,nwritten;
            nLeft = n;  
            idx = 0;
            while(nLeft > 0 )
            {
                if ( (nwritten = ::send(fd_, buffer + idx,nLeft,0)) <= 0) 
                    return nwritten;
                nLeft -= nwritten;
                idx += nwritten;
            }
            return n;
        }

        //处理ws数据
        process_state handle_ws_data() 
        {
            auto pointer = req_.data(); // 数据的起始地址
            std::size_t start = 0,end = req_.get_cur_size_();

            std::string ws_body;
            //LOG_DEBUG("handle_ws_data, req_.size %d,%d,%d",start,end,end-start);

            do {

                /*---------- ws_header ----------*/
                auto len  = end - start; // TODO
                auto ret  = ws_.parse_header(pointer + start, len);

                if (ret >=   parse_status::complete  /*0*/ ) { //完成了  do_nothing
                    start += ret; //更新开始的位置
                    len = end - start;
                }
                else if (ret == parse_status::not_complete) {
                    return process_state::need_read;
                }
                else { //发生错误
                    req_.call_event(data_proc_state::data_error);
                    Close(); //结束 TODO 应该在哪里结束?
                    return process_state::need_write;
                }
                /*---------- ws_header end ----------*/

                /*---------- ws_body ----------*/
                std::string payload;
                ws_frame_type wsft = ws_.parse_payload(pointer + start, len, payload); //解析payload
                if (wsft == ws_frame_type::WS_INCOMPLETE_FRAME) { // 末完成的frame
                    // 重新去读取, TODO 把已经处理得到的数据存起来, 显然定义一个新类成员变量 last_ws_str_
                    //req_.update_size(bytes_transferred);
                    //req_.reduce_left_body_size(bytes_transferred);
                    //continue_work_ = &connection::do_read_websocket_data;
                    //return TO_EPOLL_READ; //继续去读
                    return process_state::need_read;
                }
                if (wsft == ws_frame_type::WS_INCOMPLETE_TEXT_FRAME || wsft == ws_frame_type::WS_INCOMPLETE_BINARY_FRAME) {
                    last_ws_str_.append(std::move(payload));
                }

                //进行处理
                //根据结果
                if (!handle_ws_frame(wsft, std::move(payload), 0))
                    return process_state::need_write;

            } while ( ws_.is_fin() == false );

            req_.set_current_size(0);
            //LOG_DEBUG("after set current_size ,%d",req_.get_cur_size_());
            return process_state::need_read;
        }

        process_state do_read_websocket_head()
        {
            size_t length = req_.current_size(); //TODO 读取的长度 ?
            int ret = ws_.parse_header(req_.buffer(), length); //解析头
            //req_.set_current_size(0);

            if (ret == parse_status::complete) { //完成了
                //read payload
                auto payload_length = ws_.payload_length();
                req_.set_body_len(payload_length);
                if (req_.at_capacity(payload_length)) { //超过最大容量
                    req_.call_event(data_proc_state::data_error);
                    Close(); //结束 TODO 应该在哪里结束?
                    return process_state::need_write;
                }
                req_.set_current_size(0); // 测试完了一个frame 不应该清零
                req_.fit_size(); // TODO fit size有什么作用
                continue_work_ = &connection::do_read_websocket_data; 
            }
            else if (ret == parse_status::not_complete) {
                //req_.set_current_size(bytes_transferred); // ?
                return process_state::need_read;
                //do_read_websocket_head(ws_.left_header_len());
            }
            else { //发生错误
                req_.call_event(data_proc_state::data_error);
                Close(); //结束 TODO 应该在哪里结束?
            }
            return process_state::need_read;
        }

        //TODO
        bool ws_response_handshake_continue_work();

        process_state do_read_websocket_data() 
        {

            auto bytes_transferred = 1;
            if (req_.body_finished()) {
                req_.set_current_size(0);
                bytes_transferred = ws_.payload_length(); // TODO 长度应该如何设定?
            }

            std::string payload;
            ws_frame_type ret = ws_.parse_payload(req_.buffer(), bytes_transferred, payload); //解析payload
            if (ret == ws_frame_type::WS_INCOMPLETE_FRAME) { // 末完成的frame
                req_.update_size(bytes_transferred);
                req_.reduce_left_body_size(bytes_transferred);;
                continue_work_ = &connection::do_read_websocket_data;
                return process_state::need_read;
            }

            if (ret == ws_frame_type::WS_INCOMPLETE_TEXT_FRAME || ret == ws_frame_type::WS_INCOMPLETE_BINARY_FRAME) {
                last_ws_str_.append(std::move(payload));
            }

            //进行处理
            //根据结果
            if (!handle_ws_frame(ret, std::move(payload), bytes_transferred))
                return process_state::need_write;

            req_.set_current_size(0);
            continue_work_ =  &connection::do_read_websocket_head;
            return process_state::need_read;
        }

        bool handle_ws_frame(ws_frame_type ret, std::string&& payload, size_t bytes_transferred)
        {
            switch (ret)
            {
                case netcore::ws_frame_type::WS_ERROR_FRAME:
                    req_.call_event(data_proc_state::data_error);
                    Close(); /// TODO 关闭
                    return false;
                case netcore::ws_frame_type::WS_OPENING_FRAME:
                    break;
                case netcore::ws_frame_type::WS_TEXT_FRAME:
                case netcore::ws_frame_type::WS_BINARY_FRAME:
                    {
                        //reset_timer();
                        std::string temp;
                        if (!last_ws_str_.empty()) {
                            temp = std::move(last_ws_str_);
                        }
                        temp.append(std::move(payload));
                        req_.set_part_data(temp);
                        req_.call_event(data_proc_state::data_continue);
                    }
                    //on message
                    break;
                case netcore::ws_frame_type::WS_CLOSE_FRAME:
                    {
                        //LOG_DEBUG("handle_ws_frame type : ws_frame_type::WS_CLOSE_FRAME");
                        close_frame close_frame = ws_.parse_close_payload(payload.data(), payload.length());
                        const int MAX_CLOSE_PAYLOAD = 123;
                        size_t len = std::min<size_t>(MAX_CLOSE_PAYLOAD, payload.length());
                        req_.set_part_data({ close_frame.message, len });
                        req_.call_event(data_proc_state::data_close); // 调用

                        std::string close_msg = ws_.format_close_payload(opcode::close, close_frame.message, len);
                        auto header = ws_.format_header(close_msg.length(), opcode::close);
                        WS_manager::get_instance().send_ws_string(GetFd(), header+close_msg,true);
                        return false;
                        //send_msg(std::move(header), std::move(close_msg),true); //发送
                    }
                    break;
                case netcore::ws_frame_type::WS_PING_FRAME:
                    {
                        auto header = ws_.format_header(payload.length(), opcode::pong);
                        //send_msg(std::move(header), std::move(payload));
                        WS_manager::get_instance().send_ws_string(GetFd(), header+payload);
                    }
                    break;
                case netcore::ws_frame_type::WS_PONG_FRAME:
                    ws_ping();
                    break;
                default:
                    break;
            }

            return true;
        }


        //TODO
        void ws_ping() {}
        //-------------web socket----------------//

        //-------------chunked(read chunked not support yet, write chunked is ok)----------------------//
        void handle_chunked(size_t bytes_transferred) {
            int ret = req_.parse_chunked(bytes_transferred);
            if (ret == parse_status::has_error) {
                response_back(status_type::internal_server_error, "not support yet");
                return;
            }
        }

        /**
         * TODO
         * 重复执行的route会有性能损失
         * 最好的方法是记录 直接要执行的route,不用筛选
         */
        process_state continue_write_then_route(){
            call_back();// 去route
            return process_state::need_write;
        }

        //-------------chunked(read chunked not support yet, write chunked is ok)----------------------//

        process_state handle_body() {
            if (req_.at_capacity()) {
                response_back(status_type::bad_request, "The body is too long, limitation is 3M");
                return process_state::need_write;
            }

            bool r = handle_gzip();
            if (!r) {
                response_back(status_type::bad_request, "gzip uncompress error");
                return process_state::need_write;
            }

            if (req_.get_content_type() == content_type::multipart) {
                bool has_error = parse_multipart(req_.header_len(), req_.current_size() - req_.header_len());
                if (has_error) {
                    response_back(status_type::bad_request, "mutipart error");
                    return process_state::need_write;
                }
                return process_state::need_write;
            }

            if (req_.body_len()>0&&!req_.check_request()) {
                response_back(status_type::bad_request, "request check error");
                return process_state::need_write;
            }

            call_back(); //调用handler_request_
            return process_state::need_write;

        }

        bool handle_gzip() {
            if (req_.has_gzip()) {
                return req_.uncompress();
            }

            return true;
        }

        void response_back(status_type status, std::string&& content) {
            res_.set_status_and_content(status, std::move(content));
        }

        void response_back(status_type status) {
            res_.set_status_and_content(status);
            //disable_keep_alive();
        }


        void check_keep_alive() {
            auto req_conn_hdr = req_.get_header_value("connection");
            if (req_.is_http11()) {
                keep_alive_ = req_conn_hdr.empty() || !iequal(req_conn_hdr.data(), req_conn_hdr.size(), "close");
            }
            else {
                keep_alive_ = !req_conn_hdr.empty() && iequal(req_conn_hdr.data(), req_conn_hdr.size(), "keep-alive");
            }

            //LOG_DEBUG(" req_conn_hdr ",req_conn_hdr.data());
            //LOG_DEBUG("check_keep_alive : %d ",keep_alive_);

            res_.set_keep_alive(keep_alive_);

            if (keep_alive_) {
                // TODO
                is_upgrade_ = ws_.is_upgrade(req_);
            }
        }

        //-----------------send message----------------//
        void send_msg(std::string&& data) {
            //std::lock_guard<std::mutex> lock(buffers_mtx_);
            buffers_[active_buffer_ ^ 1].push_back(std::move(data)); // move input data to the inactive buffer
            if (!writing())
                do_write_msg();
        }

        void send_msg(std::string&& header, std::string&& data) {
            WS_manager::get_instance().send_ws_string(GetFd(),header+data);
            //std::lock_guard<std::mutex> lock(buffers_mtx_);
            //buffers_[active_buffer_ ^ 1].push_back(std::move(header));
            //buffers_[active_buffer_ ^ 1].push_back(std::move(data)); // move input data to the inactive buffer
            //if (!writing())
                //do_write_msg();
        }

        void do_write_msg() {
            active_buffer_ ^= 1; // switch buffers
            for (const auto& data : buffers_[active_buffer_]) {
                direct_write(data.c_str(),data.length()); // 写
            }
            buffers_[active_buffer_].clear(); //清空
            buffer_seq_.clear();    //清空
        }

        bool writing() const { return !buffer_seq_.empty(); } // buffer_seq_ 不空的时候是在写

        template<typename F1, typename F2>
        void set_callback(F1&& f1, F2&& f2) {
            send_ok_cb_ = std::move(f1);
            send_failed_cb_ = std::move(f2);
        }
        //-----------------send message----------------//
public:
        //-----------------HttpConn function----------------//
        void init(int fd, const sockaddr_in& addr) {
            if ( !has_continue_workd() ){ //新的
                req_.set_conn(this->shared_from_this()); // ?
                //userCount++;
                //addr_ = addr;
                fd_ = fd;
                //writeBuff_.RetrieveAll();
                //readBuff_.RetrieveAll();
                has_closed_ = false;
                reset(); // 每一次都必须是一次完整的http请求
            }
            //LOG_INFO("Client[%d](%s:%d) in, userCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
        }

        //关闭
        void Close() {
            if (has_closed_) {
                return;
            }
            reset(); //这里不reset 因为关闭的时间reset可能会导致失败 TODO
            req_.close_upload_file();
            has_closed_ = true;
            has_shake_  = false;
            continue_work_ = nullptr;
            //LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
        }



        bool IsKeepAlive() const {
            //LOG_INFO("keep_alive_ %d\n",keep_alive_);
            return keep_alive_ && is_upgrade_;
        }

        bool has_continue_workd(){
            return continue_work_ != nullptr;
        }

        void clear_continue_workd(){
            continue_work_ = nullptr;
        }

        void set_file_tag(std::shared_ptr<std::ifstream> ftag){
            m_file_tag = ftag;
        }
        auto get_file_tag(){
            return m_file_tag;
        }

    private:
        // 这里本应该是读取与写入
        // 但是为了代码的简单
        // 把读取与写入放到了connection的外部
        // connection 只进行数据的处理

    private:
        /*---------- member ----------*/

        //文件指针
        std::shared_ptr<std::ifstream> m_file_tag = nullptr;

        bool enable_timeout_ = true;
        response res_; //响应
        request  req_; //请求
        websocket ws_;
        bool is_upgrade_ = false;
        bool keep_alive_ = false;
        const std::size_t MAX_REQ_SIZE_{3*1024*1024}; //请求的最大大小,包括上传文件
        const long KEEP_ALIVE_TIMEOUT_{60*10};
        std::string& static_dir_;
        bool has_shake_ = false;

        //for writing message
        std::vector<std::string> buffers_[2]; // a double buffer
        std::vector<char> buffer_seq_; // TODO unuse
        int active_buffer_ = 0;
        std::function<void()> send_ok_cb_ = nullptr;
        std::function<void(const int&)> send_failed_cb_ = nullptr;

        std::string last_ws_str_;

        std::string chunked_header_; //响应时如果 chunked_header_ 的内容
        multipart_reader multipart_parser_;
        bool is_multi_part_file_;

        //callback handler to application layer
        http_handler* http_handler_ = nullptr;
        http_handler_check * http_handler_check_ = nullptr;
        ws_connection_check * ws_connection_check_ = nullptr;

        std::function<bool(request& req, response& res)>* upload_check_ = nullptr;
        std::function<void(request&, std::string&)> multipart_begin_ = nullptr;

        size_t last_transfer_ = 0;  //最后转化了多少字节
        //bool isFirstRead{true};
        using continue_work = process_state(connection::*)();
        continue_work continue_work_ = nullptr;

        std::atomic_bool has_closed_ = false;

        NativeSocket fd_{-1};
};

    inline constexpr data_proc_state ws_open    = data_proc_state::data_begin;
    inline constexpr data_proc_state ws_close   = data_proc_state::data_close;
    inline constexpr data_proc_state ws_error   = data_proc_state::data_error;
    inline constexpr data_proc_state ws_message = data_proc_state::data_continue;
}
