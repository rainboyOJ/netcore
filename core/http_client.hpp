#pragma once

#include <cstdio>
#include <string>
#include <vector>
#include <tuple>
#include <future>
#include <atomic>
#include <fstream>
#include <mutex>
#include <deque>
#include <string_view>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>


#include "use_asio.hpp"
#include "uri.hpp"
#include "http_parser.hpp"
#include "itoa_jeaiii.hpp"
#include "modern_callback.h"

template<typename... Args>
void log_impl(Args&&... args){
    ( (std::cerr << args ),...);
    std::cerr << std::endl;
}

#define log(...) log_impl(__FILE__ ," line: ",__LINE__," ",__VA_ARGS__);


enum error_code : int{
    EC_FAIL = 1,
    EC_in_progress = 2,
EC_invalid_argument = 3
};



// TCP客户端类
class CTcpClient
{
    public:
        int m_sockfd;

        CTcpClient():m_sockfd{0} {};

        // 向服务器发起连接，serverip-服务端ip，port通信端口
        bool ConnectToServer(const char *serverip,const int port){
            m_sockfd = socket(AF_INET,SOCK_STREAM,0); // 创建客户端的socket

            struct hostent* h; // ip地址信息的数据结构
            if ( (h=gethostbyname(serverip))==0 )
            { ::close(m_sockfd); m_sockfd=0; return false; }

            // 把服务器的地址和端口转换为数据结构
            struct sockaddr_in servaddr;
            memset(&servaddr,0,sizeof(servaddr));
            servaddr.sin_family = AF_INET;
            servaddr.sin_port = htons(port);
            memcpy(&servaddr.sin_addr,h->h_addr,h->h_length);

            // 向服务器发起连接请求
            if (connect(m_sockfd,(struct sockaddr *)&servaddr,sizeof(servaddr))!=0)
            { ::close(m_sockfd); m_sockfd=0; return false; }

            return true;
        }
        // 向对端发送报文
        int  Send(const void *buf,const int buflen){
            return send(m_sockfd,buf,buflen,0);
        }

        int Writen(const char *buffer,const size_t n)
        {
            int nLeft,idx,nwritten;
            nLeft = n;  
            idx = 0;
            while(nLeft > 0 )
            {
                if ( (nwritten = send(m_sockfd, buffer + idx,nLeft,0)) <= 0) 
                    return nwritten;
                nLeft -= nwritten;
                idx += nwritten;
            }
            return n;
        }

        // 接收对端的报文
        int  Recv(void *buf,const int buflen){
            return recv(m_sockfd,buf,buflen,0);
        }
        size_t get_recved_len(){
            return m_recv_str_buf.length();
        }

        std::tuple<bool,std::string>
        readN(int Siz){
            std::string recv_str(m_recv_str_buf);
            while ( Siz > 0 ) {
                int numBytesRead = Recv(buf,sizeof(buf));
                if( numBytesRead > 0 ){
                    for(int i=0;i<=numBytesRead-1;++i) recv_str += buf[i];
                    Siz -= numBytesRead;
                }
                else return {false,""};
            }
            //log("m_recv_str_buf",m_recv_str_buf);
            //log("recv_str",recv_str);
            return {true,recv_str};
        }

        std::tuple<bool,std::string>
        read_until(std::string_view lit){
            std::string recv_str(m_recv_str_buf);
            while ( 1 ) {
                log("start read");
                int numBytesRead = Recv(buf,sizeof(buf));
                log("numBytesRead ",numBytesRead);

                auto endsWith= [&](){
                    if( recv_str.length() < lit.length()) return false;
                    auto it = recv_str.rbegin();
                    for(int j = lit.length() - 1 ;j >=0 ;--j,++it){
                        if( lit[j] != *it) 
                            return false;
                    }
                    return true;
                };
                if( numBytesRead > 0 ){
                    //检查是否有这个位置
                    for(int i=0;i<=numBytesRead-1;++i){
                        recv_str += buf[i];
                        //std::cout << buf[i];
                        if( endsWith() ){
                            m_recv_str_buf = std::string(buf+i+1,buf+numBytesRead);
                            //log("m_recv_str_buf",m_recv_str_buf);
                            return {true,recv_str};
                        }
                    }
                }
                else {
                    return {false,""};
                }
            }
            return {true,recv_str};
        }

        void close(){
            if( m_sockfd != 0) {
                ::close(m_sockfd);
                m_sockfd = 0;
            }
        }
        ~CTcpClient() { close(); }

        std::string m_recv_str_buf;
        char buf[1024]{0};
};

namespace rojcpp {
    struct response_data {
        int ec;
        int status;
        std::string resp_body;
        std::pair<phr_header*, size_t> resp_headers;
    };
    using callback_t = std::function<void(response_data)>;

    inline static std::string INVALID_URI        = "invalid_uri";
    inline static std::string REQUEST_TIMEOUT    = "request timeout";
    inline static std::string MULTIPLE_REQUEST   = "last async request not finished";
    inline static std::string METHOD_ERROR       = "method error";
    inline static std::string INVALID_FILE_PATH  = "invalid file path";
    inline static std::string OPEN_FAILED        = "open file failed";
    inline static std::string FILE_SIZE_ERROR    = "filesize error";
    inline static std::string RESP_PARSE_ERROR   = "http response parse error";
    inline static std::string INVALID_CHUNK_SIZE = "invalid chunk size";
    inline static std::string READ_TIMEOUT       = "read timeout";

    class http_client : public std::enable_shared_from_this<http_client> {
    public:
        http_client() {
            future_ = read_close_finished_.get_future();
        }

        ~http_client() {
            close();
        }

        response_data get(std::string uri) {
            return request(http_method::GET, std::move(uri), req_content_type::json, timeout_seconds_);
        }

        response_data get(std::string uri, size_t seconds) {
            return request(http_method::GET, std::move(uri), req_content_type::json, seconds);
        }

        response_data get(std::string uri, req_content_type type) {
            return request(http_method::GET, std::move(uri), type, timeout_seconds_);
        }

        response_data get(std::string uri, size_t seconds, req_content_type type) {
            return request(http_method::GET, std::move(uri), type, seconds);
        }

        response_data get(std::string uri, req_content_type type, size_t seconds) {
            return request(http_method::GET, std::move(uri), type, seconds);
        }

        response_data post(std::string uri, std::string body) {
            return request(http_method::POST, std::move(uri), req_content_type::json, timeout_seconds_, std::move(body));
        }

        response_data post(std::string uri, std::string body, req_content_type type) {
            return request(http_method::POST, std::move(uri), type, timeout_seconds_, std::move(body));
        }

        response_data post(std::string uri, std::string body, size_t seconds) {
            return request(http_method::POST, std::move(uri), req_content_type::json, seconds, std::move(body));
        }

        response_data post(std::string uri, std::string body, req_content_type type, size_t seconds) {
            return request(http_method::POST, std::move(uri), type, seconds, std::move(body));
        }

        response_data post(std::string uri, std::string body, size_t seconds, req_content_type type) {
            return request(http_method::POST, std::move(uri), type, seconds, std::move(body));
        }

        response_data request(http_method method, std::string uri, req_content_type type = req_content_type::json, size_t seconds = 15, std::string body = "") {
            promise_ = std::make_shared<std::promise<response_data>>();
            sync_ = true;
            if (!chunked_result_.empty()) {
                chunked_result_.clear();
            }

            async_request(method, std::move(uri), nullptr, type, seconds, std::move(body));
            auto future = promise_->get_future();
            auto status = future.wait_for(std::chrono::seconds(seconds));
            in_progress_ = false;
            if (status == std::future_status::timeout) {
                promise_ = nullptr;
                return {EC_invalid_argument, 404, REQUEST_TIMEOUT };
            }
            auto result = future.get();
            promise_ = nullptr;
            return result;
        }

        template<typename _Callback>
        auto async_get(std::string uri, _Callback&& cb) {
            return async_request(http_method::GET, std::move(uri), std::forward<_Callback>(cb), req_content_type::json, timeout_seconds_);
        }

        template<typename _Callback>
        auto async_get(std::string uri, req_content_type type, _Callback&& cb) {
            return async_request(http_method::GET, std::move(uri), std::forward<_Callback>(cb), type, timeout_seconds_);
        }

        template<typename _Callback>
        auto async_get(std::string uri, _Callback&& cb, req_content_type type) {
            return async_request(http_method::GET, std::move(uri), std::forward<_Callback>(cb), type, timeout_seconds_);
        }

        template<typename _Callback>
        auto async_get(std::string uri, _Callback&& cb, size_t seconds) {
            return async_request(http_method::GET, std::move(uri), std::forward<_Callback>(cb), req_content_type::json, seconds);
        }

        template<typename _Callback>
        auto async_get(std::string uri, _Callback&& cb, req_content_type type, size_t seconds) {
            return async_request(http_method::GET, std::move(uri), std::forward<_Callback>(cb), type, seconds);
        }

        template<typename _Callback>
        auto async_get(std::string uri, _Callback&& cb, size_t seconds, req_content_type type) {
            return async_request(http_method::GET, std::move(uri), std::forward<_Callback>(cb), type, seconds);
        }

        template<typename _Callback>
        auto async_get(std::string uri, size_t seconds, _Callback&& cb) {
            return async_request(http_method::GET, std::move(uri), std::forward<_Callback>(cb), req_content_type::json, seconds);
        }

        template<typename _Callback>
        auto async_get(std::string uri, req_content_type type, size_t seconds, _Callback&& cb) {
            return async_request(http_method::GET, std::move(uri), std::forward<_Callback>(cb), type, seconds);
        }

        template<typename _Callback>
        auto async_get(std::string uri, size_t seconds, req_content_type type, _Callback&& cb) {
            return async_request(http_method::GET, std::move(uri), std::forward<_Callback>(cb), type, seconds);
        }

        template<typename _Callback>
        auto async_post(std::string uri, std::string body, _Callback&& cb) {
            return async_request(http_method::POST, std::move(uri), std::forward<_Callback>(cb), req_content_type::json, timeout_seconds_, std::move(body));
        }

        template<typename _Callback>
        auto async_post(std::string uri, std::string body, _Callback&& cb, req_content_type type) {
            return async_request(http_method::POST, std::move(uri), std::forward<_Callback>(cb), type, timeout_seconds_, std::move(body));
        }

        template<typename _Callback>
        auto async_post(std::string uri, std::string body, _Callback&& cb, size_t seconds) {
            return async_request(http_method::POST, std::move(uri), std::forward<_Callback>(cb), req_content_type::json, seconds, std::move(body));
        }

        template<typename _Callback>
        auto async_post(std::string uri, std::string body, _Callback&& cb, req_content_type type, size_t seconds) {
            return async_request(http_method::POST, std::move(uri), std::forward<_Callback>(cb), type, seconds, std::move(body));
        }

        template<typename _Callback>
        auto async_post(std::string uri, std::string body, _Callback&& cb, size_t seconds, req_content_type type) {
            return async_request(http_method::POST, std::move(uri), std::forward<_Callback>(cb), type, seconds, std::move(body));
        }

        template<typename _Callback>
        auto async_post(std::string uri, std::string body, req_content_type type, _Callback&& cb) {
            return async_request(http_method::POST, std::move(uri), std::forward<_Callback>(cb), type, timeout_seconds_, std::move(body));
        }

        template<typename _Callback>
        auto async_post(std::string uri, std::string body, size_t seconds, _Callback&& cb) {
            return async_request(http_method::POST, std::move(uri), std::forward<_Callback>(cb), req_content_type::json, seconds, std::move(body));
        }

        template<typename _Callback>
        auto async_post(std::string uri, std::string body, req_content_type type, size_t seconds, _Callback&& cb) {
            return async_request(http_method::POST, std::move(uri), std::forward<_Callback>(cb), type, seconds, std::move(body));
        }

        template<typename _Callback>
        auto async_post(std::string uri, std::string body, size_t seconds, req_content_type type, _Callback&& cb) {
            return async_request(http_method::POST, std::move(uri), std::forward<_Callback>(cb), type, seconds, std::move(body));
        }

        template<typename _Callable_t>
        auto async_request(http_method method, std::string uri, _Callable_t&& cb, req_content_type type = req_content_type::json, size_t seconds = 15, std::string body = "")
        ->MODERN_CALLBACK_RESULT(void(response_data)){
            MODERN_CALLBACK_TRAITS(cb, void(response_data));
            async_request_impl(method, std::move(uri), MODERN_CALLBACK_CALL(), type, seconds, std::move(body));
            MODERN_CALLBACK_RETURN();
        }
        void async_request_impl(http_method method, std::string uri, callback_t cb, req_content_type type = req_content_type::json, size_t seconds = 15, std::string body = "") {
            bool need_switch = false;
            if (!promise_) {//just for async request, guard continuous async request, it's not allowed; async request must be after last one finished!
                need_switch = sync_;
                sync_ = false;
            }

            if (in_progress_) {
                set_error_value(cb, EC_in_progress, MULTIPLE_REQUEST);
                return;
            }
            else {
                in_progress_ = true;
            }

            if (method != http_method::POST && !body.empty()) {
                set_error_value(cb, EC_invalid_argument, METHOD_ERROR);
                return;
            }

            bool init = last_domain_.empty();
            bool need_reset = need_switch || (!init && (uri.find(last_domain_) == std::string::npos));
            
            if (need_reset) {
                close(false);

                //wait for read close finish
                future_.wait();
                read_close_finished_ = {};
                future_ = read_close_finished_.get_future();

                //reset_socket();
            }

            auto [r, u] = get_uri(uri);
            if (!r) {
                set_error_value(cb, EC_invalid_argument, INVALID_URI);          
                return;
            }

            last_domain_ = std::string(u.schema).append("://").append(u.host);
            timeout_seconds_ = seconds;
            req_content_type_ = type;
            cb_ = std::move(cb);
            context ctx(u, method, std::move(body));
            if (promise_) {
                weak_ = promise_; //weak_ 指向 promise_
            }
            if (has_connected_) {
                do_write(std::move(ctx));
            }
            else {
                async_connect(std::move(ctx));
            }            
        }

        template<typename _Callable_t>
        auto download(std::string src_file, std::string dest_file, _Callable_t&& cb, size_t seconds = 60) {
            return download(std::move(src_file), std::move(dest_file), 0, std::move(cb), seconds);
        }

        template<typename _Callable_t>
        auto download(std::string src_file, std::string dest_file, int64_t size, _Callable_t&& cb, size_t seconds = 60)
            ->MODERN_CALLBACK_RESULT(void(response_data)) {
            MODERN_CALLBACK_TRAITS(cb, void(response_data));
            download_impl(std::move(src_file), std::move(dest_file), size, MODERN_CALLBACK_CALL(), seconds);
            MODERN_CALLBACK_RETURN();
        }

        void download_impl(std::string src_file, std::string dest_file, int64_t size, callback_t cb, size_t seconds = 60) {
            auto parant_path = fs::path(dest_file).parent_path();
            std::error_code code;
            fs::create_directories(parant_path, code);
            if (code) {
                cb_ = std::move(cb);
                callback(EC_invalid_argument, INVALID_FILE_PATH);
                return;
            }

            if (size > 0) {
                char buffer[20];
                auto p = cinatra::i64toa_jeaiii(size, buffer);
                add_header("cinatra_start_pos", std::string(buffer, p - buffer));
            }
            else {
                int64_t file_size = fs::file_size(dest_file, code);
                if (!code && file_size > 0) {
                    char buffer[20];
                    auto p = cinatra::i64toa_jeaiii(file_size, buffer);
                    add_header("cinatra_start_pos", std::string(buffer, p - buffer));
                }
            }

            download_file_ = std::make_shared<std::ofstream>(dest_file, std::ios::binary | std::ios::app);
            if (!download_file_->is_open()) {
                cb_ = std::move(cb);
                callback(EC_invalid_argument, OPEN_FAILED);
                return;
            }

            if (size > 0) {
                char buffer[20];
                auto p = cinatra::i64toa_jeaiii(size, buffer);
                add_header("cinatra_start_pos", std::string(buffer, p - buffer));
            }
            else {
                int64_t file_size = fs::file_size(dest_file, code);
                if (!code && file_size > 0) {
                    char buffer[20];
                    auto p = cinatra::i64toa_jeaiii(file_size, buffer);
                    add_header("cinatra_start_pos", std::string(buffer, p - buffer));
                }
            }

            async_get(std::move(src_file), std::move(cb), req_content_type::none, seconds);
        }

        void download(std::string src_file, std::function<void(int, std::string_view)> chunk, size_t seconds = 60) {
            on_chunk_ = std::move(chunk);
            async_get(std::move(src_file), nullptr, req_content_type::none, seconds);
        }

        template<typename _Callable_t>
        auto upload(std::string uri, std::string filename, _Callable_t&& cb, size_t seconds = 60) {
            return upload(std::move(uri), std::move(filename), 0, std::forward<_Callable_t>(cb), seconds);
        }

        /**
         * @brief 上传size个字符的字符,当成文件上传
         */
        template<typename _Callable_t>
        auto upload_string(std::string uri, std::string filename,std::size_t size, _Callable_t&& cb, size_t seconds = 60) {
            multipart_str_ = std::move(filename);
            memFileSize = size;
            return async_request(http_method::POST, uri, std::forward<_Callable_t>(cb), req_content_type::multipart, seconds, "");
        }

        template<typename _Callable_t>
        auto upload(std::string uri, std::string filename, size_t start, _Callable_t&& cb, size_t seconds = 60) {
            multipart_str_ = std::move(filename);
            start_ = start;
            return async_request(http_method::POST, uri, std::forward<_Callable_t>(cb), req_content_type::multipart, seconds, "");
        }

        void add_header(std::string key, std::string val) {
            if (key.empty())
                return;

            if (key == "Host")
                return;

            headers_.emplace_back(std::move(key), std::move(val));
        }

        void add_header_str(std::string pair_str) {
            if (pair_str.empty())
                return;

            if (pair_str.find("Host:") != std::string::npos)
                return;

            header_str_.append(pair_str);
        }

        void clear_headers() {
            if (!headers_.empty()) {
                headers_.clear();
            }

            if (!header_str_.empty()) {
                header_str_.clear();
            }
        }

        std::pair<phr_header*, size_t> get_resp_headers() {
            if (!copy_headers_.empty())
                parser_.set_headers(copy_headers_);

            return parser_.get_headers();
        }

        std::string_view get_header_value(std::string_view key) {
            return parser_.get_header_value(key);
        }


    private:
        void callback(const int ec) {
            callback(ec, 404, "");
        }

        void callback(const int ec, std::string error_msg) {
            callback(ec, 404, std::move(error_msg));
        }

        void callback(const int ec, int status) {
            callback(ec, status, "");
        }

        void callback(const int ec, int status, std::string_view result) {
            if (auto sp = weak_.lock(); sp) {
                sp->set_value({ ec, status, std::string(result), get_resp_headers() });
                weak_.reset();
                return;
            }

            if (cb_) { //有回调函数
                cb_({ ec, status, std::string(result), get_resp_headers() });
                cb_ = nullptr;
            }

            if (on_chunk_) { //处理 chunk_
                on_chunk_(ec, result);
            }

            in_progress_ = false;
        }

        std::pair<bool, uri_t> get_uri(const std::string& uri) {
            uri_t u;
            if (!u.parse_from(uri.data())) {
                if (u.schema.empty())
                    return { false, {} };

                auto new_uri = url_encode(uri);

                if (!u.parse_from(new_uri.data())) {
                    return { false, {} };
                }
            }

            if (u.schema == "https"sv) {
                assert(false);
            }

            return { true, u };
        }

        void async_connect(context ctx) {
            reset_timer();
            bool succ = client_.ConnectToServer(ctx.host.c_str(), atoi(ctx.port.c_str()));
            if (!succ) {                   
                callback(EC_FAIL);
                client_.close();
                return;
            }

            has_connected_ = true;
            do_read_write(ctx);
        }

        void do_read_write(const context& ctx) {
            int error_ignored;
            do_write(ctx);
            do_read();
        }

        void do_write(const context& ctx) {
            if (req_content_type_ == req_content_type::multipart) {
                send_multipart_msg(ctx);
            }
            else {
                send_msg(ctx);
            }
        }

        void send_msg(const context& ctx) {
            write_msg_ = build_write_msg(ctx);
            Write(write_msg_, [this, self = shared_from_this()](const int& ec, const size_t length) {
                if (ec) {
                    callback(ec);
                    close();
                    return;
                }
            });
        }

        void send_multipart_msg(const context& ctx) {
            auto filename = std::move(multipart_str_);
            if(memFileSize != 0){ // 此时生成数据
                auto left_file_size = memFileSize;
                header_str_.append("Content-Type: multipart/form-data; boundary=").append(BOUNDARY);
                auto multipart_str = multipart_file_start(fs::path(filename).filename().string());
                auto write_str = build_write_msg(ctx, total_multipart_size(left_file_size, multipart_str.size()));
                write_str.append(multipart_str);
                multipart_str_ = std::move(write_str);
                //multipart_str_. append()
                for(std::size_t i = 0 ;i < memFileSize ;++i)
                    multipart_str_.append("a");
                multipart_str_.append(MULTIPART_END);
                //log("multipart_str_",multipart_str_);
                send_mem_data(); //改成发送内存数据
                return;
            }

            auto file = std::make_shared<std::ifstream>(filename, std::ios::binary);
            if (!file) {
                callback(EC_invalid_argument, INVALID_FILE_PATH);
                return;
            }

            std::error_code ec;
            size_t size = fs::file_size(filename, ec);
            if (ec || start_ == -1) {
                file->close();
                callback(EC_invalid_argument, FILE_SIZE_ERROR);
                return;
            }

            if (start_ > 0) {
                file->seekg(start_);
            }

            auto left_file_size = size - start_;
            header_str_.append("Content-Type: multipart/form-data; boundary=").append(BOUNDARY);
            auto multipart_str = multipart_file_start(fs::path(filename).filename().string());
            auto write_str = build_write_msg(ctx, total_multipart_size(left_file_size, multipart_str.size()));
            write_str.append(multipart_str);
            multipart_str_ = std::move(write_str);
            log("multipart_str_",multipart_str_);
            send_file_data(std::move(file));
        }

        std::string build_write_msg(const context& ctx, size_t content_len = 0) {
            std::string write_msg(method_name(ctx.method));
            //can be optimized here
            write_msg.append(" ").append(ctx.path);
            if (!ctx.query.empty()) {
                write_msg.append("?").append(ctx.query);
            }
            write_msg.append(" HTTP/1.1\r\nHost:").
                append(ctx.host).append("\r\n");

            if (header_str_.find("Content-Type") == std::string::npos) {
                auto type_str = get_content_type_str(req_content_type_);
                if (!type_str.empty()) {
                    headers_.emplace_back("Content-Type", std::move(type_str));
                }
            }

            bool has_connection = false;
            //add user header
            if (!headers_.empty()) {
                for (auto& pair : headers_) {
                    if (pair.first == "Connection") {
                        has_connection = true;
                    }
                    write_msg.append(pair.first).append(": ").append(pair.second).append("\r\n");
                }
            }

            if (!header_str_.empty()) {
                if (header_str_.find("Connection")!=std::string::npos) {
                    has_connection = true;
                }
                write_msg.append(header_str_).append("\r\n");
            }

            //add content
            if (!ctx.body.empty()) {
                char buffer[20];
                auto p = cinatra::i32toa_jeaiii((int)ctx.body.size(), buffer);

                write_msg.append("Content-Length: ").append(buffer, p - buffer).append("\r\n");
            }
            else {
                if (ctx.method == http_method::POST) {
                    if (content_len > 0) {
                        char buffer[20];
                        auto p = cinatra::i32toa_jeaiii((int)content_len, buffer);

                        write_msg.append("Content-Length: ").append(buffer, p - buffer).append("\r\n");
                    }
                    else {
                        write_msg.append("Content-Length: 0\r\n");
                    }
                }
            }

            if (!has_connection) {
                write_msg.append("Connection: keep-alive\r\n");
            }

            write_msg.append("\r\n");

            if (!ctx.body.empty()) {
                write_msg.append(std::move(ctx.body));
            }

            return write_msg;
        }

        void do_read() {
            reset_timer();
            auto __header__ = client_.read_until("\r\n\r\n");
            cancel_timer();
            if(std::get<0>(__header__) == false){
                log("读取失败");
                close();
                return;
            }
            else {
                std::cout << "read_header" << std::endl;
                //std::cout << std::get<1>(__header__) << std::endl;
                //callback(0,200,std::get<1>(__header__));
                auto & headers = std::get<1>(__header__);
                int ret = parser_.parse_response(headers.c_str(),headers.size(),0);
                if( ret < 0){
                    callback(EC_invalid_argument, 404,
                            RESP_PARSE_ERROR);
                    close();
                    return;
                }

                bool is_chunked = parser_.is_chunked();
                std::cout << "body_len: " ;
                std::cout << parser_.body_len() << std::endl;
                if( parser_.body_len() == 0){
                    callback(0,parser_.status());
                    close();
                    return;
                }

                size_t content_len = (size_t)parser_.body_len();
                size_t size_to_read = content_len - client_.get_recved_len();
                copy_headers(); // TODO ????
                do_read_body(false,parser_.status(),size_to_read);
            }
            /*
            async_read_until(TWO_CRCF, [this, self = shared_from_this()](auto ec, size_t size) {
                cancel_timer();
                if (!ec) {
                    //parse header
                    const char* data_ptr = boost::asio::buffer_cast<const char*>(read_buf_.data());
                    size_t buf_size = read_buf_.size();
                    int ret = parser_.parse_response(data_ptr, size, 0);
                    read_buf_.consume(size);
                    if (ret < 0) {
                        callback(EC_invalid_argument, 404,
                            RESP_PARSE_ERROR);
                        if (buf_size > size) {
                            read_buf_.consume(buf_size - size);
                        }

                        read_or_close(parser_.keep_alive());
                        return;
                    }

                    bool is_chunked = parser_.is_chunked();

                    if (is_chunked) {
                        copy_headers();
                        //read_chunk_header
                        read_chunk_head(parser_.keep_alive());
                    }
                    else {
                        if (parser_.body_len() == 0) {
                            callback(ec, parser_.status());

                            read_or_close(parser_.keep_alive());
                            return;
                        }

                        size_t content_len = (size_t)parser_.body_len();
                        if ((size_t)parser_.total_len() <= buf_size) {
                            callback(ec, parser_.status(), { data_ptr + parser_.header_len(), content_len });
                            read_buf_.consume(content_len);

                            read_or_close(parser_.keep_alive());
                            return;
                        }

                        size_t size_to_read = content_len - read_buf_.size();
                        copy_headers();
                        do_read_body(parser_.keep_alive(), parser_.status(), size_to_read);
                    }
                }
                else {
                    callback(ec);
                    close();

                    //read close finish
                    if(!is_ready())
                        read_close_finished_.set_value(true);
                }
            });
            */
        }

        bool is_ready() {
            return future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
        }

        void do_read_body(bool keep_alive, int status, size_t size_to_read) {
            reset_timer();
            auto body = client_.readN(size_to_read);
            callback(0,status,std::get<1>(body));
            //async_read(size_to_read, [this, self = shared_from_this(), keep_alive, status](auto ec, size_t size) {
                //cancel_timer();
                //if (!ec) {
                    //size_t data_size = read_buf_.size();
                    //const char* data_ptr = boost::asio::buffer_cast<const char*>(read_buf_.data());

                    //callback(ec, status, { data_ptr, data_size });

                    //read_buf_.consume(data_size);

                    //read_or_close(keep_alive);
                //}
                //else {
                    //callback(ec);
                    //close();
                //}
            //});
        }

        void read_or_close(bool keep_alive) {
            if (keep_alive) {
                do_read();
            }
            else {
                close();
            }
        }

        void read_chunk_head(bool keep_alive) {
            reset_timer();
            //async_read_until(CRCF, [this, self = shared_from_this(), keep_alive](auto ec, size_t size) {
                //cancel_timer();
                //if (!ec) {
                    //size_t buf_size = read_buf_.size();
                    //const char* data_ptr = boost::asio::buffer_cast<const char*>(read_buf_.data());
                    //std::string_view size_str(data_ptr, size - CRCF.size());
                    //auto chunk_size = hex_to_int(size_str);
                    //if (chunk_size < 0) {
                        //callback(EC_invalid_argument, 404,
                            //INVALID_CHUNK_SIZE);
                        //read_or_close(keep_alive);
                        //return;
                    //}

                    //read_buf_.consume(size);

                    //if (chunk_size == 0) {
                        //if (read_buf_.size() < CRCF.size()) {
                            //read_buf_.consume(read_buf_.size());
                            //read_chunk_body(keep_alive, CRCF.size() - read_buf_.size());
                        //}
                        //else {
                            //read_buf_.consume(CRCF.size());
                            //read_chunk_body(keep_alive, 0);
                        //}
                        //return;
                    //}

                    //if ((size_t)chunk_size <= read_buf_.size()) {
                        //const char* data = boost::asio::buffer_cast<const char*>(read_buf_.data());
                        //append_chunk(std::string_view(data, chunk_size));
                        //read_buf_.consume(chunk_size + CRCF.size());
                        //read_chunk_head(keep_alive);
                        //return;
                    //}

                    //size_t extra_size = read_buf_.size();
                    //size_t size_to_read = chunk_size - extra_size;
                    //const char* data = boost::asio::buffer_cast<const char*>(read_buf_.data());
                    //append_chunk({ data, extra_size });
                    //read_buf_.consume(extra_size);

                    //read_chunk_body(keep_alive, size_to_read + CRCF.size());
                //}
                //else {
                    //callback(ec);
                    //close();
                //}
            //});
        }

        void read_chunk_body(bool keep_alive, size_t size_to_read) {
            reset_timer();
            //async_read(size_to_read, [this, self = shared_from_this(), keep_alive](auto ec, size_t size) {
                //cancel_timer();
                //if (!ec) {
                    //if (size <= CRCF.size()) {
                        ////finish all chunked
                        //read_buf_.consume(size);
                        //callback(ec, 200, chunked_result_);
                        //clear_chunk_buffer();
                        //do_read();
                        //return;
                    //}

                    //size_t buf_size = read_buf_.size();
                    //const char* data_ptr = boost::asio::buffer_cast<const char*>(read_buf_.data());
                    //append_chunk({ data_ptr, size - CRCF.size() });
                    //read_buf_.consume(size);
                    //read_chunk_head(keep_alive);
                //}
                //else {
                    //callback(ec);
                    //close();
                //}
            //});
        }

        void append_chunk(std::string_view chunk) {
            if (on_chunk_) {
                on_chunk_({}, chunk);
                return;
            }

            if (download_file_) {
                download_file_->write(chunk.data(), chunk.size());
            }
            else {
                chunked_result_.append(chunk);
            }
        }

        void clear_chunk_buffer() {
            if (download_file_) {
                download_file_->close();
            }
            else {
                if (!sync_&&!chunked_result_.empty()) {
                    chunked_result_.clear();
                }
            }
        }

        template<typename Handler>
        void async_read(size_t size_to_read, Handler handler) {
                //boost::asio::async_read(socket_, read_buf_, boost::asio::transfer_exactly(size_to_read), std::move(handler));
        }

        template<typename Handler>
        void async_read_until(const std::string& delim, Handler handler) {
            //boost::asio::async_read_until(socket_, read_buf_, delim, std::move(handler));
        }

        //写数据
        template<typename Handler>
        void Write(const std::string& msg, Handler handler) {
            //boost::asio::Write(socket_, boost::asio::buffer(msg), std::move(handler));
            int ret = client_.Writen(msg.c_str(), msg.length());
            if( ret < 0 ){
                log("写入时发生错误");
                callback(EC_FAIL);
            }
            //if( handler != nullptr)
                //handler();
        }

        void close(bool close_ssl = true) {
            int ec;
            if (!has_connected_)
                return;

            has_connected_ = false;
            //timer_.cancel(ec);
            client_.close();
        }

        void reset_timer() {
            //if (timeout_seconds_ == 0 || promise_) {
                //return;
            //}

            //auto self(this->shared_from_this());
            //timer_.expires_from_now(std::chrono::seconds(timeout_seconds_));
            //timer_.async_wait([this, self](const int& ec) {
                //if (ec || sync_) {
                    //return;
                //}

                //close(false); //don't close ssl now, close ssl when read/write error
                //if (download_file_) {
                    //download_file_->close();
                //}
            //});
        }

        void cancel_timer() {
            //if (!cb_) {
                //return; //just for async request
            //}

            //if (timeout_seconds_ == 0 || promise_) {
                //return;
            //}

            //timer_.cancel();
        }

        bool is_ssl() const {
            return false;
        }

        void send_file_data(std::shared_ptr<std::ifstream> file) {
            auto eof = make_upload_data(*file);
            if (eof) {
                return;
            }

            auto self = this->shared_from_this();
            Write(multipart_str_, [this, self, file = std::move(file)](int ec, std::size_t length) mutable {
                if (!ec) {
                    multipart_str_.clear();
                    send_file_data(std::move(file));
                }
                else {
                    on_chunk_(ec, "send failed");
                    close();
                }
            });
        }
        void send_mem_data(){
            //multipart_str_.resize(size);
            //std::fill(multipart_str_.begin(),multipart_str_.end(),'a');
            Write(multipart_str_, [this](int ec,std::size_t length){
                    if(!ec){
                        multipart_str_.clear();
                    }
                    else {
                        on_chunk_(ec, "send failed");
                        close();
                    }
                });
        }

        std::string multipart_file_start(std::string filename) {
            std::string multipart_start;
            multipart_start.append("--" + BOUNDARY + CRCF);
            multipart_start.append("Content-Disposition: form-data; name=\"" + std::string("test") + "\"; filename=\"" + std::move(filename) + "\"" + CRCF);
            multipart_start.append(CRCF);
            return multipart_start;
        }

        size_t total_multipart_size(size_t left_file_size, size_t multipart_start_size) {
            return left_file_size + multipart_start_size + MULTIPART_END.size();
        }

        bool make_upload_data(std::ifstream& file) {
            bool eof = file.peek() == EOF;
            if (eof) {
                file.close();
                return true;//finish all file
            }

            std::string content;
            const size_t size = 3 * 1024 * 1024;
            content.resize(size);
            file.read(&content[0], size);
            int64_t read_len = (int64_t)file.gcount();
            assert(read_len > 0);
            eof = file.peek() == EOF;

            if (read_len < size) {
                content.resize(read_len);
            }

            multipart_str_.append(content);
            if (eof) {
                multipart_str_.append(MULTIPART_END);
            }

            return false;
        }

        void copy_headers() {
            if (!copy_headers_.empty()) {
                copy_headers_.clear();
            }
            auto [headers, num_headers] = parser_.get_headers();
            for (size_t i = 0; i < num_headers; i++) {
                copy_headers_.emplace_back(std::string(headers[i].name, headers[i].name_len),
                    std::string(headers[i].value, headers[i].value_len));
            }
        }


        void set_error_value(const callback_t& cb,error_code  ec, const std::string& error_msg) {
            if (promise_) {
                promise_->set_value({ EC_invalid_argument, 404, error_msg });
            }
            if (cb) {
                cb({ EC_invalid_argument, 404, error_msg });
            }
            read_close_finished_ = {};
        }

    private:
        std::atomic_bool has_connected_ = false;
        callback_t cb_;
        std::atomic_bool in_progress_ = false;

        std::size_t timeout_seconds_ = 60;

        http_parser parser_;
        std::vector<std::pair<std::string, std::string>> copy_headers_;
        std::string header_str_;
        std::vector<std::pair<std::string, std::string>> headers_;
        req_content_type req_content_type_ = req_content_type::json;

        std::string write_msg_;

        std::string chunked_result_;
        std::shared_ptr<std::ofstream> download_file_ = nullptr;
        std::function<void(int, std::string_view)> on_chunk_ = nullptr;

        std::string multipart_str_;
        size_t start_;

        std::string last_domain_;
        std::promise<bool> read_close_finished_;
        std::future<bool> future_;
        long long memFileSize{-1}; //-1表示上传的文件是普通文件,否则是上传的生成的字符文件的大小

        std::shared_ptr<std::promise<response_data>> promise_ = nullptr;
        std::weak_ptr<std::promise<response_data>> weak_;
        bool sync_ = false;
        CTcpClient client_; //封装的socket服务器
    };
}
