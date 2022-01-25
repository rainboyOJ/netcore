// 有关文件下载的函数
#include "hs_utils.h"

namespace rojcpp {
    

std::string get_send_data(request& req, const size_t len){
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

void write_ranges_header(request& req, std::string_view mime, std::string filename, std::string file_size){
    std::string header_str = "HTTP/1.1 200 OK\r\nAccess-Control-Allow-origin: *\r\nAccept-Ranges: bytes\r\n"; header_str.append("Content-Disposition: attachment;filename=");
    header_str.append(std::move(filename)).append("\r\n"); //下载的文件名
    header_str.append("Connection: keep-alive\r\n");
    header_str.append("Content-Type: ").append(mime).append("\r\n");//类型
    header_str.append("Content-Length: "); //长度
    header_str.append(file_size).append("\r\n\r\n"); //文件头结束
    LOG_DEBUG("write_ranges_header :%s \n\n",header_str.c_str());
    req.get_conn()->write_ranges_header(std::move(header_str)); //存入
}

void write_ranges_data(request& req) {
    LOG_DEBUG("============ write_ranges_data");
    const size_t len = 3 * 1024 * 1024; // 默认3MB
    auto str = get_send_data(req, len);
    LOG_DEBUG("============ write_ranges_data %s\n",str.c_str());
    auto read_len = str.size();
    bool eof = (read_len == 0 || read_len != len); // 是否结束
    req.get_conn()->write_ranges_data(std::move(str), eof);
}

void write_chunked_header(request& req, std::shared_ptr<std::ifstream> in, std::string_view mime) {
    auto range_header = req.get_header_value("range");
    req.set_range_flag(!range_header.empty()); //分块发送
    req.set_range_start_pos(range_header);

    std::string res_content_header = std::string(mime.data(), mime.size()) + "; charset=utf8";
    res_content_header += std::string("\r\n") + std::string("Access-Control-Allow-origin: *");
    res_content_header += std::string("\r\n") + std::string("Accept-Ranges: bytes");
    if constexpr (__config__::staticResCacheMaxAge > 0)
    {
        const std::string max_age = std::string("max-age=") + std::to_string(__config__::staticResCacheMaxAge);
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


void write_chunked_body(request& req) {
    const size_t len = 3 * 1024 * 1024;
    auto str = get_send_data(req, len); //得到发送的数据
    auto read_len = str.size();
    bool eof = (read_len == 0 || read_len != len);
    req.get_conn()->write_chunked_data(std::move(str), eof);
}


bool is_small_file(std::ifstream* in,request& req) {
    auto file_begin = in->tellg();
    in->seekg(0, std::ios_base::end);
    auto  file_size = in->tellg();
    in->seekg(file_begin);
    req.save_request_static_file_size(file_size);
    return file_size <= 5 * 1024 * 1024; //5MB 以下是小文件
}

void send_small_file(response& res, std::ifstream* in, std::string_view mime) {
    res.add_header("Access-Control-Allow-origin", "*");
    res.add_header("Content-type", std::string(mime.data(), mime.size()) + "; charset=utf8");
    std::stringstream file_buffer;
    file_buffer << in->rdbuf(); //读取所有
    if constexpr (__config__::staticResCacheMaxAge>0)
    {
        const std::string max_age = std::string("max-age=") + std::to_string(__config__::staticResCacheMaxAge);
        res.add_header("Cache-Control", max_age.data());
    }
#ifdef CINATRA_ENABLE_GZIP //TODO
    res.set_status_and_content(status_type::ok, file_buffer.str(), res_content_type::none, content_encoding::gzip);
#else
    res.set_status_and_content(status_type::ok, file_buffer.str());
#endif
}

} // end namespace rojcpp
