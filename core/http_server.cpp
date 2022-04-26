// 有关文件下载的函数
#include "http_server.hpp"

namespace netcore {

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
        
        std::string relative_file_name = req.get_relative_filename();
        std::string fullpath = std::string(__config__::static_dir) + relative_file_name;
        process_download(fullpath, req, res);

    });
}


} // end namespace netcore
