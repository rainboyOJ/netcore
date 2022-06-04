#include "http_server.hpp"
#include <iostream>

using namespace netcore;

__START_LOG__

int main(){

#ifdef __NETCORE__DEBUG__
    std::cout << "debug" << std::endl;
#else
    std::cout << "no debug" << std::endl;
#endif
    LOG(INFO) << "hello word info \n" ;
    LOG(DEBUG) << "hello word info \n" ;

    http_server hs(2,8899);
    hs.set_http_handler<GET>("/hello_word", [](request & req, response &res){
            res.set_status_and_content(status_type::ok, "hello world");
            });
    hs.run();

    return 0;
}
