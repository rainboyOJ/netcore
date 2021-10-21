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

#include "log.h"

#include "request.hpp" //请求
#include "response.hpp" //响应
//#include "websocket.hpp" //websocket
#include "define.h"
#include "http_cache.hpp"
#include "multipart_reader.hpp"
#include "buffer.h"

namespace rojcpp {
	using http_handler        = std::function<void(request&, response&)>;
	using send_ok_handler     = std::function<void()>;
	using send_failed_handler = std::function<void(const int&)>;

    class base_connection {
    public:
        virtual ~base_connection() {}
    };

	class HttpConn :public base_connection {
	public:
        virtual void init(int sockFd, const sockaddr_in& addr) = 0;
        virtual ssize_t read(int* saveErrno) =0;
        virtual ssize_t write(int* saveErrno) =0 ;
        virtual void Close() =0;
        int GetFd() const { return fd_;}
        int GetPort() const { return addr_.sin_port; }
        const char* GetIP() const{ return inet_ntoa(addr_.sin_addr); }
        sockaddr_in GetAddr() const{ return addr_; }
        virtual bool process() = 0;
        virtual int ToWriteBytes() = 0;

        bool IsKeepAlive() const { return 0; }

        static bool isET;
        //static const char* srcDir;
        static std::atomic<int> userCount;

        virtual ~HttpConn() override {}

    protected:

        int fd_;
        struct  sockaddr_in addr_;
        bool isClose_;
        int iovCnt_; // TODO
        struct iovec iov_[2];
        Buffer readBuff_; // 读缓冲区
        Buffer writeBuff_; // 写缓冲区
	};


    using SocketType = int;
	class connection : 
	    public HttpConn,private noncopyable ,public std::enable_shared_from_this<connection>
	{
	private:

	public:
        //connection() :
            //req_(res_)
        //{}
			//MAX_REQ_SIZE_{3*1024*1024},
			//KEEP_ALIVE_TIMEOUT_{60*10}
		//{}

		connection(connection&& rv_conn)
		    :MAX_REQ_SIZE_(rv_conn.MAX_REQ_SIZE_),
            KEEP_ALIVE_TIMEOUT_(rv_conn.KEEP_ALIVE_TIMEOUT_),
			http_handler_(rv_conn.http_handler_),
            static_dir_(rv_conn.static_dir_),
            upload_check_(rv_conn.upload_check_),
            res_(std::move(rv_conn.res_)),
		    req_(res_)
		{}

		explicit connection(std::size_t max_req_size, long keep_alive_timeout,
			http_handler& handler, std::string& static_dir, std::function<bool(request& req, response& res)>* upload_check):
			MAX_REQ_SIZE_(max_req_size), KEEP_ALIVE_TIMEOUT_(keep_alive_timeout),
			http_handler_(handler), req_(res_), static_dir_(static_dir), upload_check_(upload_check)
		{
			init_multipart_parser();
		}


        auto& socket() {
            return socket_;
        }

        std::string local_address() { //本地地址
            if (has_closed_) {
                return "";
            }

            //std::stringstream ss;
            return "unknown"; // TODO fix
        }

        std::string remote_address() { //远程地址
            if (has_closed_) {
                return "";
            }
            return "unknown"; // TODO fix
        }

        std::pair<std::string, std::string> remote_ip_port() { //远程ip port``
            std::string remote_addr = remote_address();
            if (remote_addr.empty())
                return {};

            size_t pos = remote_addr.find(':');
            std::string ip = remote_addr.substr(0, pos);
            std::string port = remote_addr.substr(pos + 1);
            return {std::move(ip), std::move(port)};
        }

		void start() {          //开始
            req_.set_conn(this->shared_from_this()); // ?
			do_read();  //读取
		}

		const std::string& static_dir() {
			return static_dir_;
		}

		void on_error(status_type status, std::string&& reason) {
			keep_alive_ = false;
			req_.set_state(data_proc_state::data_error);
			response_back(status, std::move(reason));
		}

		void on_close() {
			keep_alive_ = false;
			req_.set_state(data_proc_state::data_error);
			close();
		}

		void reset_timer() {
			if (!enable_timeout_)
				return;
			//TODO fix
		}

		void cancel_timer() {
			if (!enable_timeout_)
				return;
			// TODO fix
		}

		void enable_timeout(bool enable) {
			enable_timeout_ = enable;
		}

		void set_tag(std::any&& tag) { // TODO tag 有什么用？
			tag_ = std::move(tag);
		}

		auto& get_tag() {
			return tag_;
		}

		//============ web socket ============
		//template<typename... Fs>
		//void send_ws_string(std::string msg, Fs&&... fs) {
			//send_ws_msg(std::move(msg), opcode::text, std::forward<Fs>(fs)...);
		//}

		//template<typename... Fs>
		//void send_ws_binary(std::string msg, Fs&&... fs) {
			//send_ws_msg(std::move(msg), opcode::binary, std::forward<Fs>(fs)...);
		//}

		//template<typename... Fs>
		//void send_ws_msg(std::string msg, opcode op = opcode::text, Fs&&... fs) {
			//constexpr const size_t size = sizeof...(Fs);
			//static_assert(size != 0 || size != 2);
			//if constexpr(size == 2) {
				//set_callback(std::forward<Fs>(fs)...);
			//}

			//auto header = ws_.format_header(msg.length(), op);
			//send_msg(std::move(header), std::move(msg));
		//}
		//============ web socket ============

		void write_chunked_header(std::string_view mime,bool is_range=false) {
			req_.set_http_type(content_type::chunked);
			reset_timer();
			if(!is_range){
                chunked_header_ = http_chunk_header + "Content-Type: " + std::string(mime.data(), mime.length()) + "\r\n\r\n";
			}else{
                chunked_header_ = http_range_chunk_header + "Content-Type: " + std::string(mime.data(), mime.length()) + "\r\n\r\n";
            }
            //异步 TODO async_write
			//boost::asio::async_write(socket(),
				//boost::asio::buffer(chunked_header_),
				//[self = this->shared_from_this()](const int& ec, std::size_t bytes_transferred) {
				//self->handle_chunked_header(ec);
			//});
			async_write(chunked_header_);
			handle_chunked_header(0);
		}

        //写一个 ranges 头 TODO ？
        void write_ranges_header(std::string header_str) {
            reset_timer();
            chunked_header_ = std::move(header_str); //reuse the variable
            //TODO async_write
            //boost::asio::async_write(socket(),
				//boost::asio::buffer(chunked_header_),
				//[this, self = this->shared_from_this()](const int& ec, std::size_t bytes_transferred) {
				//handle_chunked_header(ec);
			//});
			async_write(chunked_header_);
			handle_chunked_header(0);
        }

		//写数据 chunk_data
		void write_chunked_data(std::string&& buf, bool eof) {
			reset_timer();

			std::vector<boost::asio::const_buffer> buffers = res_.to_chunked_buffers(buf.data(), buf.length(), eof);
			if (buffers.empty()) {
				handle_write(int{});
				return;
			}

			//auto self = this->shared_from_this();
            // TODO async_write
			//boost::asio::async_write(socket(), buffers, [this, self, buf = std::move(buf), eof](const int& ec, size_t) {
				//if (ec) {
					//return;
				//}

				//if (eof) {
					//req_.set_state(data_proc_state::data_end);
				//}
				//else {
					//req_.set_state(data_proc_state::data_continue);
				//}

				//call_back();
			//});
			for (const auto& e : buffers) {
			    async_write(e);
			}
            if (eof) {
                req_.set_state(data_proc_state::data_end);
            }
            else {
                req_.set_state(data_proc_state::data_continue);
            }
            call_back();
		}

        // TODO 什么是 ranges_data
        void write_ranges_data(std::string&& buf, bool eof) {
            reset_timer();

            chunked_header_ = std::move(buf); //reuse the variable
            auto self = this->shared_from_this();
            // TODO async_write
            //boost::asio::async_write(socket(), boost::asio::buffer(chunked_header_), [this, self, eof](const int& ec, size_t) {
                //if (ec) {
                    //return;
                //}

                //if (eof) {
                    //req_.set_state(data_proc_state::data_end);
                //}
                //else {
                    //req_.set_state(data_proc_state::data_continue);
                //}

                //call_back();
            //});
			async_write(chunked_header_);
            if (eof) {
                req_.set_state(data_proc_state::data_end);
            }
            else {
                req_.set_state(data_proc_state::data_continue);
            }
            call_back();
        }

		void response_now() {
			do_write();
		}

		//设置处理函数
		void set_multipart_begin(std::function<void(request&, std::string&)> begin) {
			multipart_begin_ = std::move(begin);
		}

        void set_validate(size_t max_header_len, check_header_cb check_headers) {
            req_.set_validate(max_header_len, std::move(check_headers));
        }

        void enable_response_time(bool enable) {
            res_.enable_response_time(enable);
        }

		bool has_close() {
			return has_closed_;
		}

        response& get_res() {
            return res_;
        }

        virtual ~connection() override {
            close();
        }
	private:
		void do_read() {
            reset();
			async_read_some();
		}

        void reset() {
            last_transfer_ = 0;
            len_ = 0;
            req_.reset();
            res_.reset();
            reset_timer();
        }


		//这个有用吗
		inline void async_read_some() {
			int bytes_transferred = __read_some(req_.buffer(), req_.left_size());
			if( bytes_transferred <=0 ) close();
			handle_read(bytes_transferred);
		}

		//core 处理读取的数据
		void handle_read(std::size_t bytes_transferred) { 
			auto last_len = req_.current_size();
			last_transfer_ = last_len;
			bool at_capacity = req_.update_and_expand_size(bytes_transferred); //更新最大容量
			if (at_capacity) {  //超过最大容量
				response_back(status_type::bad_request, "The request is too long, limitation is 3M");
				return;
			}

			int ret = req_.parse_header(len_); //解析头

			if (ret == parse_status::has_error) { 
				response_back(status_type::bad_request);
				return;
			}

			check_keep_alive();
			if (ret == parse_status::not_complete) {
				do_read_head(); //继续读取 头
			}
			else {
				auto total_len = req_.total_len();
				if (bytes_transferred > total_len + 4) { //TODO ? 这个基本不会运行 或者说运行的时机我还不知道
					std::string_view str(req_.data()+ len_+ total_len, 4);
					if (str == "GET " || str == "POST") {
						handle_pipeline(total_len, bytes_transferred); // TODO ?
						return;
					}
				} //				if (req_.get_method() == "GET"&&http_cache::get().need_cache(req_.get_url())&&!http_cache::get().not_cache(req_.get_url())) {
//					handle_cache();
//					return;
//				}

				req_.set_last_len(len_); //
				handle_request(bytes_transferred);
			}
		}

		//核心 
		// 处理和各种类型的body
		// 如果没有 body 直接 和处理头
		void handle_request(std::size_t bytes_transferred) { //处理请求
			if (req_.has_body()) {
				auto type = get_content_type();
				req_.set_http_type(type);
				switch (type) {
				case rojcpp::content_type::string:
				case rojcpp::content_type::unknown:
					handle_string_body(bytes_transferred);
					break;
				case rojcpp::content_type::multipart:
					handle_multipart();
					break;
				case rojcpp::content_type::octet_stream:
					handle_octet_stream(bytes_transferred);
					break;
				case rojcpp::content_type::urlencoded:
					handle_form_urlencoded(bytes_transferred);
					break;
				case rojcpp::content_type::chunked:
					handle_chunked(bytes_transferred);
					break;
				}
			}
			else {
				handle_header_request();
			}
		}

		// cache
		void handle_cache() {
			auto raw_url = req_.raw_url();
			if (!http_cache::get().empty()) {
				auto resp_vec = http_cache::get().get(std::string(raw_url.data(), raw_url.length()));
				if (!resp_vec.empty()) {
					std::vector<boost::asio::const_buffer> buffers;
					for (auto& iter : resp_vec) {
						//buffers.emplace_back(boost::asio::buffer(iter.data(), iter.size()));
						async_write(iter);
					}
					handle_write(0);
					// TODO async_write
					//boost::asio::async_write(socket(), buffers,
						//[self = this->shared_from_this(), resp_vec = std::move(resp_vec)](const int& ec, std::size_t bytes_transferred) {
						//self->handle_write(ec);
					//});
				}
			}
		}

		// TODO ?
		// ret
		// bytes_transferred
		void handle_pipeline(size_t ret, std::size_t bytes_transferred) {
			res_.set_delay(true);
			req_.set_last_len(len_);
			handle_request(bytes_transferred);
			last_transfer_ += bytes_transferred;
			if (len_ == 0)  //len_ 表示总体长度 ？
				len_ = ret;
			else
				len_ += ret;
			
			auto& rep_str = res_.response_str();
			int result = 0;
            size_t left = ret;
			bool head_not_complete = false;
			bool body_not_complete = false;
			size_t left_body_len = 0;
			//int index = 1;
			while (true) {
				result = req_.parse_header(len_); //解析头
				if (result == -1) {
					return;
				}

				if (result == -2) {     //还没有解析完
					head_not_complete = true;
					break;
				}
				
				//index++;
				auto total_len = req_.total_len();

				if (total_len <= (bytes_transferred - len_)) {
					req_.set_last_len(len_);
					handle_request(bytes_transferred);
				}				

				len_ += total_len;

				if (len_ == last_transfer_) {
					break;
				}
				else if (len_ > last_transfer_) {
					auto n = len_ - last_transfer_;
					len_ -= total_len;
					if (n<req_.header_len()) {
						head_not_complete = true;
					}
					else {
						body_not_complete = true;
						left_body_len = n;
					}

					break;
				}
			}

			res_.set_delay(false);
			//TODO async_write
			//boost::asio::async_write(socket(), boost::asio::buffer(rep_str.data(), rep_str.size()),
				//[head_not_complete, body_not_complete, left_body_len, this,
				//self = this->shared_from_this(), &rep_str](const int& ec, std::size_t bytes_transferred) {
				//rep_str.clear();
				//if (head_not_complete) {
					//do_read_head();
					//return;
				//}

				//if (body_not_complete) {
					//req_.set_left_body_size(left_body_len);
					//do_read_body();
					//return;
				//}

				//handle_write(ec);
			//});
		}

		//读取头
		void do_read_head() {
			reset_timer();
			int bytes_transferred =  __read_some(req_.buffer(), req_.left_size());
			handle_read(bytes_transferred);
			//socket().async_read_some(boost::asio::buffer(req_.buffer(), req_.left_size()),
				//[this, self = this->shared_from_this()](const int& e, std::size_t bytes_transferred) {
			//});
		}

		//读取body
		void do_read_body() {
			reset_timer();

			auto self = this->shared_from_this();
			// TODO do read_some
			//boost::asio::async_read(socket(), boost::asio::buffer(req_.buffer(), req_.left_body_len()),
				//[this, self](const int& ec, size_t bytes_transferred) {
				//if (ec) {
					////LOG_WARN << ec.message();
					//close();
					//return;
				//}

				//req_.update_size(bytes_transferred);
				//req_.reduce_left_body_size(bytes_transferred);

				//if (req_.body_finished()) {
					//handle_body();
				//}
				//else {
					//do_read_body();
				//}
			//});
		}

		//写 回应
		void do_write() {
			reset_timer();
			
			std::string& rep_str = res_.response_str();
			if (rep_str.empty()) {
				handle_write(int{});
				return;
			}

			//cache
//			if (req_.get_method() == "GET"&&http_cache::get().need_cache(req_.get_url()) && !http_cache::get().not_cache(req_.get_url())) {
//				auto raw_url = req_.raw_url();
//				http_cache::get().add(std::string(raw_url.data(), raw_url.length()), res_.raw_content());
//			}
			
            //TODO async_write
			//boost::asio::async_write(socket(), boost::asio::buffer(rep_str.data(), rep_str.size()),
				//[this, self = this->shared_from_this()](const int& ec, std::size_t bytes_transferred) {
				//handle_write(ec);
			//});

		}

		void handle_write(const int& ec) {
			if (ec) {
				return;
			}

			if (keep_alive_) {
				do_read();
			}
			else {
                reset();
				cancel_timer(); //avoid close two times
				shutdown();
				close();
			}
		}

		//得到body type
		content_type get_content_type() {
			if (req_.is_chunked())
				return content_type::chunked;

			auto content_type = req_.get_header_value("content-type");
			if (!content_type.empty()) {
				if (content_type.find("application/x-www-form-urlencoded") != std::string_view::npos) {
					return content_type::urlencoded;
				}
				else if (content_type.find("multipart/form-data") != std::string_view::npos) {
					auto size = content_type.find("=");
					auto bd = content_type.substr(size + 1, content_type.length() - size);
					if (bd[0] == '"'&& bd[bd.length()-1] == '"') {
						bd = bd.substr(1, bd.length() - 2);
					}
					std::string boundary(bd.data(), bd.length());
					multipart_parser_.set_boundary("\r\n--" + std::move(boundary));
					return content_type::multipart;
				}
				else if (content_type.find("application/octet-stream") != std::string_view::npos) {
					return content_type::octet_stream;
				}
				else {
					return content_type::string;
				}
			}

			return content_type::unknown;
		}

		void close(bool close_ssl = true) {
#ifdef rojcpp_ENABLE_SSL
            if (close_ssl && ssl_stream_) {
                int ec;
                ssl_stream_->shutdown(ec);
                ssl_stream_ = nullptr;
            }
#endif
            if (has_closed_) {
                return;
            }

			req_.close_upload_file();
            shutdown();
			int ec;
			//socket_.close(ec);
			has_closed_ = true;
            has_shake_ = false;
		}

		/****************** begin handle http body data *****************/
		void handle_string_body(std::size_t bytes_transferred) {
			//defalt add limitation for string_body and else. you can remove the limitation for very big string.
			if (req_.at_capacity()) {
				response_back(status_type::bad_request, "The request is too long, limitation is 3M");
				return;
			}

			if (req_.has_recieved_all()) {
				handle_body();
			}
			else {
				req_.expand_size();
				assert(req_.current_size() >= req_.header_len());
				size_t part_size = req_.current_size() - req_.header_len();
				if (part_size == -1) {
					response_back(status_type::internal_server_error);
					return;
				}
				req_.reduce_left_body_size(part_size);
				do_read_body();
			}
		}

		//-------------octet-stream----------------//
		void handle_octet_stream(size_t bytes_transferred) {
			//call_back();
			try {
                auto tp = std::chrono::high_resolution_clock::now();
                auto nano = tp.time_since_epoch().count();
                std::string name = static_dir_ + "/" + std::to_string(nano);
				req_.open_upload_file(name);
			}
			catch (const std::exception& ex) {
				response_back(status_type::internal_server_error, ex.what());
				return;
			}

			req_.set_state(data_proc_state::data_continue);//data
			size_t part_size = bytes_transferred - req_.header_len();
			if (part_size != 0) {
				req_.reduce_left_body_size(part_size);
				req_.set_part_data({ req_.current_part(), part_size });
				req_.write_upload_data(req_.current_part(), part_size);
				//call_back();
			}

			if (req_.has_recieved_all()) {
				//on finish
				req_.set_state(data_proc_state::data_end);
				call_back();
				do_write();
			}
			else {
				req_.fit_size();
				req_.set_current_size(0);
				do_read_octet_stream_body();
			}
		}

		void do_read_octet_stream_body() {
			auto self = this->shared_from_this();
			// TODO
			//boost::asio::async_read(socket(), boost::asio::buffer(req_.buffer(), req_.left_body_len()),
				//[this, self](const int& ec, size_t bytes_transferred) {
				//if (ec) {
					//req_.set_state(data_proc_state::data_error);
					//call_back();
					//close();
					//return;
				//}

				//req_.set_part_data({ req_.buffer(), bytes_transferred });
				//req_.write_upload_data(req_.buffer(), bytes_transferred);
				////call_back();

				//req_.reduce_left_body_size(bytes_transferred);

				//if (req_.body_finished()) {
					//req_.set_state(data_proc_state::data_end);
					//call_back();
					//do_write();
				//}
				//else {
					//do_read_octet_stream_body();
				//}
			//});
		}

		//-------------octet-stream----------------//

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
				do_read_form_urlencoded();
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
			if (!res_.need_delay())
				do_write();
		}

		void do_read_form_urlencoded() {
			reset_timer();

			auto self = this->shared_from_this();
			// TODO
			//boost::asio::async_read(socket(), boost::asio::buffer(req_.buffer(), req_.left_body_len()),
				//[this, self](const int& ec, size_t bytes_transferred) {
				//if (ec) {
					////LOG_WARN << ec.message();
					//close();
					//return;
				//}

				//req_.update_size(bytes_transferred);
				//req_.reduce_left_body_size(bytes_transferred);

				//if (req_.body_finished()) {
					//handle_url_urlencoded_body();
				//}
				//else {
					//do_read_form_urlencoded();
				//}
			//});
		}
		//-------------form urlencoded----------------//

		void call_back() {
			assert(http_handler_);
			http_handler_(req_, res_);
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
					if (req_.get_state() == data_proc_state::data_error)
						return;
					if(is_multi_part_file_)
					{
						req_.close_upload_file(); //关闭最后一个文件
						auto pfile = req_.get_file();
						if (pfile) {
							auto old_name = pfile->get_file_path();
							auto pos = old_name.rfind("_ing");
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

		//解析 multipart
		bool parse_multipart(size_t size, std::size_t length) {
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
				//LOG_WARN << multipart_parser_.get_error_message();
				req_.set_state(data_proc_state::data_error);
				return true;
			}

			req_.reduce_left_body_size(length);
			return false;
		}

		//处理multipart
		void handle_multipart() {
			if (upload_check_) {
				bool r = (*upload_check_)(req_, res_);
				if (!r) {
					close();
					return;
				}					
			}

			bool has_error = parse_multipart(req_.header_len(), req_.current_size() - req_.header_len());

			if (has_error) {
				response_back(status_type::bad_request, "mutipart error");
				return;
			}

			if (req_.has_recieved_all_part()) {
				call_back();
				do_write();
			}
			else {
				req_.set_current_size(0);
				do_read_multipart();
			}
		}

		//读multipart
		void do_read_multipart() {
			reset_timer();

			req_.fit_size();
			auto self = this->shared_from_this();
			//TODO
			//boost::asio::async_read(socket(), boost::asio::buffer(req_.buffer(), req_.left_body_len()),
				//[self, this](int ec, std::size_t length) {
				//if (ec) {
					//req_.set_state(data_proc_state::data_error);
					//call_back();
					//response_back(status_type::bad_request, "mutipart error");
					//return;
				//}

				//bool has_error = parse_multipart(0, length);

				//if (has_error) { //parse error
					//keep_alive_ = false;
					//response_back(status_type::bad_request, "mutipart error");
					//return;
				//}

				//reset_timer();
				//if (req_.body_finished()) {
					//call_back();
					//do_write();
					//return;
				//}

				//req_.set_current_size(0);
				//do_read_part_data();
			//});
		}

		//读取一部分数据
		void do_read_part_data() {
			auto self = this->shared_from_this();
			// TODO
			//boost::asio::async_read(socket(), boost::asio::buffer(req_.buffer(), req_.left_body_size()),
				//[self, this](int ec, std::size_t length) {
				//if (ec) {
					//req_.set_state(data_proc_state::data_error);
					//call_back();
					//return;
				//}

				//bool has_error = parse_multipart(0, length);

				//if (has_error) {
					//response_back(status_type::bad_request, "mutipart error");
					//return;
				//}

				//reset_timer();
				//if (!req_.body_finished()) {
					//do_read_part_data();
				//}
				//else {
					////response_back(status_type::ok, "multipart finished");
					//call_back();
					//do_write();
				//}
			//});
		}
		//-------------multipart----------------------//

		void handle_header_request() {
			if (is_upgrade_) { //websocket
				req_.set_http_type(content_type::websocket);
				//timer_.cancel();
				//ws_.upgrade_to_websocket(req_, res_);
				response_handshake();
				return;
			}

			bool r = handle_gzip();
			if (!r) {
				response_back(status_type::bad_request, "gzip uncompress error");
				return;
			}

			call_back(); // ?

			if (req_.get_content_type() == content_type::chunked)
				return;

			if (req_.get_state() == data_proc_state::data_error) {
				return;
			}

			if (!res_.need_delay())
				do_write();
		}

		//-------------web socket----------------//
		void response_handshake() {
			std::vector<boost::asio::const_buffer> buffers = res_.to_buffers();
			if (buffers.empty()) {
				close();
				return;
			}

			auto self = this->shared_from_this();
			// TODO
			//boost::asio::async_write(socket(), buffers, [this, self](const int& ec, std::size_t length) {
				//if (ec) {
					//close();
					//return;
				//}

				//req_.set_state(data_proc_state::data_begin);
				//call_back();
				//req_.call_event(req_.get_state());

				//req_.set_current_size(0);
				//do_read_websocket_head(SHORT_HEADER);
			//});
		}

        //web socket
        /*
		void do_read_websocket_head(size_t length) {
			auto self = this->shared_from_this();
			boost::asio::async_read(socket(), boost::asio::buffer(req_.buffer(), length),
				[this, self](const int& ec, size_t bytes_transferred) {
				if (ec) {
					cancel_timer();
					req_.call_event(data_proc_state::data_error);

					close();
					return;
				}

				size_t length = bytes_transferred + req_.current_size();
				req_.set_current_size(0);
				int ret = ws_.parse_header(req_.buffer(), length);

				if (ret == parse_status::complete) {
					//read payload
					auto payload_length = ws_.payload_length();
					req_.set_body_len(payload_length);
					if (req_.at_capacity(payload_length)) {
						req_.call_event(data_proc_state::data_error);
						close();
						return;
					}

					req_.set_current_size(0);
					req_.fit_size();
					do_read_websocket_data(req_.left_body_len());
				}
				else if (ret == parse_status::not_complete) {
					req_.set_current_size(bytes_transferred);
					do_read_websocket_head(ws_.left_header_len());
				}
				else {
					req_.call_event(data_proc_state::data_error);
					close();
				}
			});
		}

		void do_read_websocket_data(size_t length) {
			auto self = this->shared_from_this();
			boost::asio::async_read(socket(), boost::asio::buffer(req_.buffer(), length),
				[this, self](const int& ec, size_t bytes_transferred) {
				if (ec) {
					req_.call_event(data_proc_state::data_error);
					close();
					return;
				}

				if (req_.body_finished()) {
					req_.set_current_size(0);
					bytes_transferred = ws_.payload_length();
				}

				std::string payload;
				ws_frame_type ret = ws_.parse_payload(req_.buffer(), bytes_transferred, payload);
				if (ret == ws_frame_type::WS_INCOMPLETE_FRAME) {
					req_.update_size(bytes_transferred);
					req_.reduce_left_body_size(bytes_transferred);
					do_read_websocket_data(req_.left_body_len());
					return;
				}

                if (ret == ws_frame_type::WS_INCOMPLETE_TEXT_FRAME || ret == ws_frame_type::WS_INCOMPLETE_BINARY_FRAME) {
                    last_ws_str_.append(std::move(payload));
                }

				if (!handle_ws_frame(ret, std::move(payload), bytes_transferred))
					return;

				req_.set_current_size(0);
				do_read_websocket_head(SHORT_HEADER);
			});
		}
		*/

        // web socket
        /*
		bool handle_ws_frame(ws_frame_type ret, std::string&& payload, size_t bytes_transferred) {
			switch (ret)
			{
			case rojcpp::ws_frame_type::WS_ERROR_FRAME:
				req_.call_event(data_proc_state::data_error);
				close();
				return false;
			case rojcpp::ws_frame_type::WS_OPENING_FRAME:
				break;
			case rojcpp::ws_frame_type::WS_TEXT_FRAME:
			case rojcpp::ws_frame_type::WS_BINARY_FRAME:
			{
				reset_timer();
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
			case rojcpp::ws_frame_type::WS_CLOSE_FRAME:
			{
				close_frame close_frame = ws_.parse_close_payload(payload.data(), payload.length());
				const int MAX_CLOSE_PAYLOAD = 123;
				size_t len = std::min<size_t>(MAX_CLOSE_PAYLOAD, payload.length());
				req_.set_part_data({ close_frame.message, len });
				req_.call_event(data_proc_state::data_close);

				std::string close_msg = ws_.format_close_payload(opcode::close, close_frame.message, len);
				auto header = ws_.format_header(close_msg.length(), opcode::close);
				send_msg(std::move(header), std::move(close_msg));
			}
			break;
			case rojcpp::ws_frame_type::WS_PING_FRAME:
			{
				auto header = ws_.format_header(payload.length(), opcode::pong);
				send_msg(std::move(header), std::move(payload));
			}
			break;
			case rojcpp::ws_frame_type::WS_PONG_FRAME:
				ws_ping();
				break;
			default:
				break;
			}

			return true;
		}

		void ws_ping() {
			timer_.expires_from_now(std::chrono::seconds(60));
			timer_.async_wait([self = this->shared_from_this()](int const& ec) {
				if (ec) {
					self->close(false);
					return;
				}

				self->send_ws_msg("ping", opcode::ping);
			});
		}
		*/
		//-------------web socket----------------//

		//-------------chunked(read chunked not support yet, write chunked is ok)----------------------//
		void handle_chunked(size_t bytes_transferred) {
			int ret = req_.parse_chunked(bytes_transferred);
			if (ret == parse_status::has_error) {
				response_back(status_type::internal_server_error, "not support yet");
				return;
			}
		}

		void handle_chunked_header(const int& ec) {
			if (ec) {
				return;
			}

			req_.set_state(data_proc_state::data_continue);
			call_back();//app set the data
		}
		//-------------chunked(read chunked not support yet, write chunked is ok)----------------------//

		void handle_body() {
			if (req_.at_capacity()) {
				response_back(status_type::bad_request, "The body is too long, limitation is 3M");
				return;
			}

			bool r = handle_gzip();
			if (!r) {
				response_back(status_type::bad_request, "gzip uncompress error");
				return;
			}

			if (req_.get_content_type() == content_type::multipart) {
				bool has_error = parse_multipart(req_.header_len(), req_.current_size() - req_.header_len());
				if (has_error) {
					response_back(status_type::bad_request, "mutipart error");
					return;
				}
				do_write();
				return;
			}

            if (req_.body_len()>0&&!req_.check_request()) {
                response_back(status_type::bad_request, "request check error");
                return;
            }

			call_back(); //调用handler_request_

			if (!res_.need_delay()) //不需要延迟，直接写 TODO 改成write_ EPOLLOUT 事件
				do_write();
		}

		bool handle_gzip() {
			if (req_.has_gzip()) {
				return req_.uncompress();
			}

			return true;
		}

		void response_back(status_type status, std::string&& content) {
			res_.set_status_and_content(status, std::move(content));
			do_write(); //response to client
		}

		void response_back(status_type status) {
			res_.set_status_and_content(status);
			do_write(); //response to client
		}

		enum parse_status {
			complete = 0,
			has_error = -1,
			not_complete = -2,
		};

		void check_keep_alive() {
			auto req_conn_hdr = req_.get_header_value("connection");
			if (req_.is_http11()) {
				keep_alive_ = req_conn_hdr.empty() || !iequal(req_conn_hdr.data(), req_conn_hdr.size(), "close");
			}
			else {
				keep_alive_ = !req_conn_hdr.empty() && iequal(req_conn_hdr.data(), req_conn_hdr.size(), "keep-alive");
			}

			if (keep_alive_) {
			    // TODO
				//is_upgrade_ = ws_.is_upgrade(req_);
			}
		}

		void shutdown() {
			int ignored_ec; 
			//TODO  close(fd)
            //socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
		}

		//-----------------send message----------------//
		void send_msg(std::string&& data) {
			std::lock_guard<std::mutex> lock(buffers_mtx_);
			buffers_[active_buffer_ ^ 1].push_back(std::move(data)); // move input data to the inactive buffer
			if (!writing())
				do_write_msg();
		}

		void send_msg(std::string&& header, std::string&& data) {
			std::lock_guard<std::mutex> lock(buffers_mtx_);
			buffers_[active_buffer_ ^ 1].push_back(std::move(header));
			buffers_[active_buffer_ ^ 1].push_back(std::move(data)); // move input data to the inactive buffer
			if (!writing())
				do_write_msg();
		}

		void do_write_msg() {
			active_buffer_ ^= 1; // switch buffers
			for (const auto& data : buffers_[active_buffer_]) {
				//for (const auto& e : data) { // TODO ???
                    //buffer_seq_.push_back(e);
				//}
                async_write(data);
			}

			//boost::asio::async_write(socket(), buffer_seq_, [this, self = this->shared_from_this()](const int& ec, size_t bytes_transferred) {
				std::lock_guard<std::mutex> lock(buffers_mtx_);
				buffers_[active_buffer_].clear();
				buffer_seq_.clear();

				//if (!ec) {
					if (send_ok_cb_)
						send_ok_cb_();
					if (!buffers_[active_buffer_ ^ 1].empty()) // have more work
						do_write_msg();
				//}
				//else {
					//if (send_failed_cb_)
						//send_failed_cb_(ec);
					//req_.set_state(data_proc_state::data_error);
					//call_back();
					//close();
				//}
			//});
		}

		bool noasync_write(const char * buff_){ //写一些东西
		}

		bool writing() const { return !buffer_seq_.empty(); }

		template<typename F1, typename F2>
		void set_callback(F1&& f1, F2&& f2) {
			send_ok_cb_ = std::move(f1);
			send_failed_cb_ = std::move(f2);
		}
		//-----------------send message----------------//
public:
		//-----------------HttpConn function----------------//
        virtual void init(int fd, const sockaddr_in& addr) override{
            userCount++;
            addr_ = addr;
            fd_ = fd;
            writeBuff_.RetrieveAll();
            readBuff_.RetrieveAll();
            isClose_ = false;
            LOG_INFO("Client[%d](%s:%d) in, userCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
        }

        virtual void Close() override{
            if(isClose_ == false){
                isClose_ = true; 
                userCount--;
                close(fd_);
                LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
            }
        }

        virtual ssize_t read(int* saveErrno) override { //不停的读，直到结束为止
            ssize_t len = -1;
            do {
                len = readBuff_.ReadFd(fd_, saveErrno);
                if (len <= 0) {
                    break;
                }
            } while (isET);
            return len;
        }

        virtual ssize_t write(int* saveErrno) override{
            return 1; // TODO
        }

        virtual bool process() override {
            start(); //开始读取
            return true;
        }
        virtual int ToWriteBytes() override {
            return  1;
        }

private:

		//-----------------HttpConn function end----------------//

        //读取一部分数据
        inline int __read_some(char * buff_,int size){
            return recv(socket_, buff_, size, 0);
        }

        //异步写 是写到Buff里
        inline int async_write(std::string_view str){
            writeBuff_.Append(str.data(), str.length());
            return str.length();
        }

        ssize_t write(int *saveErrno, std::string_view str){
            int siz = Writen(str.data(), str.length());
            if( siz<= 0 ) *saveErrno = siz;
            return siz;
        }

        //返回写入的长度，<=0 是错误
        int Writen(const char *buffer,const size_t n)
        {
            int nLeft,idx,nwritten;
            nLeft = n;  
            idx = 0;
            while(nLeft > 0 )
            {
                if ( (nwritten = send(socket_, buffer + idx,nLeft,0)) <= 0) 
                    return nwritten;
                nLeft -= nwritten;
                idx += nwritten;
            }
            return n;
        }
		//-----------------base function----------------//

        //boost::asio::ip::tcp::socket socket_;
		//boost::asio::steady_timer timer_;
		
		int socket_;
		bool enable_timeout_ = true;
		response res_;
		request req_;
		//websocket ws_;
		bool is_upgrade_ = false;
		bool keep_alive_ = false;
		const std::size_t MAX_REQ_SIZE_{3*1024*1024};
		const long KEEP_ALIVE_TIMEOUT_{60*10};
		std::string& static_dir_;
		bool has_shake_ = false;
		//std::atomic_bool has_closed_ = false;
		bool has_closed_ = false;

		//for writing message
		std::mutex buffers_mtx_;
		std::vector<std::string> buffers_[2]; // a double buffer
		std::vector<char> buffer_seq_; // TODO unuse
		int active_buffer_ = 0;
		std::function<void()> send_ok_cb_ = nullptr;
		std::function<void(const int&)> send_failed_cb_ = nullptr;

        std::string last_ws_str_;

		std::string chunked_header_;
		multipart_reader multipart_parser_;
		bool is_multi_part_file_;
		//callback handler to application layer
		const http_handler& http_handler_;
		std::function<bool(request& req, response& res)>* upload_check_ = nullptr;
		std::any tag_;
		std::function<void(request&, std::string&)> multipart_begin_ = nullptr;

		size_t len_ = 0;
		size_t last_transfer_ = 0;  //最后转化了多少字节

	};

	inline constexpr data_proc_state ws_open = data_proc_state::data_begin;
	inline constexpr data_proc_state ws_message = data_proc_state::data_continue;
	inline constexpr data_proc_state ws_close = data_proc_state::data_close;
	inline constexpr data_proc_state ws_error = data_proc_state::data_error;
}
