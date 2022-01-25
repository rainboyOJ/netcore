// 有关文件下载的函数
#include "http_server.hpp"

namespace rojcpp {

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
                if constexpr (__config__::transfer_type_== transfer_type::CHUNKED)
                    write_chunked_header(req, in, mime); //调用的是这个
                else
                    write_ranges_header(req, mime, fs::path(relative_file_name).filename().string(), std::to_string(fs::file_size(fullpath)));
                }
                break;
            //数据继续,就写完header 后写body?
            case rojcpp::data_proc_state::data_continue:
                {
                    if constexpr (__config__::transfer_type_ == transfer_type::CHUNKED)
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
                LOG_ERROR("rojcpp::data_proc_state::data_error");
                //network error
            }
                break;
        }
    },enable_cache{false});
}

} // end namespace rojcpp
