// 有关文件下载的函数
#include "http_server.hpp"

namespace rojcpp {
    

std::string http_server_::get_send_data(request& req, const size_t len) {
    auto conn = req.get_conn(); //得到connection 的指针
    auto in = std::any_cast<std::shared_ptr<std::ifstream>>(conn->get_tag()); //得到 in
    std::string str;
    str.resize(len);
    in->read(&str[0], len);
    size_t read_len = (size_t)in->gcount();
    if (read_len != len) {
        str.resize(read_len);
    }
    return str;
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
    //调用了connection的 write_chunked_header
    req.get_conn()->write_chunked_header(std::string_view(res_content_header),req.is_range());
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

//设置 静态资源hander
void http_server_::set_static_res_handler() {
    set_http_handler<POST,GET>(STATIC_RESOURCE, [this](request& req, response& res){
        if (download_check_) { // 如果有下载检查
            bool r = download_check_(req, res);
            if (!r) { //不成功
                res.set_status_and_content(status_type::bad_request);
                return;
            }                        
        }

        // 得到request 数据获取的状态
        // 这是一个状态机
        auto state = req.get_state(); 
        switch (state) {
            //数据开始
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

                //如果设置的发送类型为 CHUNKED
                if(transfer_type_== transfer_type::CHUNKED)
                    write_chunked_header(req, in, mime); //调用的是这个
                else
                    write_ranges_header(req, mime, fs::path(relative_file_name).filename().string(), std::to_string(fs::file_size(fullpath)));
                }
                break;
            //数据继续,就写完header 后写body?
            case rojcpp::data_proc_state::data_continue:
                {
                    if (transfer_type_ == transfer_type::CHUNKED)
                        write_chunked_body(req); //写 body
                    else
                        write_ranges_data(req);
                }
                break;
            //数据结束
            case rojcpp::data_proc_state::data_end:
                {
                    auto conn = req.get_conn();
                    conn->clear_continue_workd(); // 结束
                }
                break;
            //数据错误
            case rojcpp::data_proc_state::data_error:
            {
                //network error
            }
                break;
        }
    },enable_cache{false});
}

} // end namespace rojcpp
