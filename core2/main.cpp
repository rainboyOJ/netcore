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
    std::cout << "hello" << std::endl;
    return 0;
}
