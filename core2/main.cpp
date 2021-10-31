#include <iostream>
#include "http_server.hpp"
#include "response_cv.hpp"
#include "response.hpp"
#include "request.hpp"
#include "connection.hpp"

using namespace rojcpp;

void http_handler_f(request&,response&){
}

bool upload_check_f(request&,response&){
    return 1;
}

int main(){

    //http_handler k = http_handler_f;
    
    //std::string dir = "static_dir";
    //connection c(
////std::size_t max_req_size,
////long keep_alive_timeout,
////http_handler& handler,
////std::string& static_dir,
////std::function<bool(request& req, response& res)>* upload_check
            //100000,100000,
            //k,
            //dir,
            ////&upload_check_f
            //nullptr
            //);
    http_server_ htp;
    htp.init(8090, 1, 100000, false, 
            3306,"root", "root", "webserver", 
            12, 6, true, 0, 1024);


    htp.set_http_handler<GET>("/", [](request& req, response& res) mutable{
        res.set_status_and_content(status_type::ok, "hello world");
        //res.set_status_and_content(status_type::ok, std::move(str));
    });

    htp.set_http_handler<POST>("/upload", [](request& req, response& res) mutable{
            std::cout << req.get_file()->get_file_path() << std::endl;
            std::cout << req.get_file()->get_file_size() << std::endl;
        res.set_status_and_content(status_type::ok, "hello world");
        //res.set_status_and_content(status_type::ok, std::move(str));
    });
    htp.run();
    std::cout << "hello" << std::endl;
    return 0;
}
