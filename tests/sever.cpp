#include <regex>
#include "http_server.hpp"

//创建一个全局的服务器
rojcpp::http_server_ Server;
int main(){
    using namespace rojcpp;
    Server.set_http_handler<GET>("/helloworld",[](request & req,response & res){
            res.set_status_and_content(status_type::ok,"hello world,this rojcpp Server",req_content_type::string);
    });

    //while_char
    Server.set_http_handler<GET>("/hello/*",[](request & req,response & res){
            res.set_status_and_content(status_type::ok,"hello world,Wild route",req_content_type::string);
        });

    //regex_route
    std::regex regex_route("/regex/(\\w+)/(\\d+)");

    Server.set_http_regex_handler<GET>(regex_route, [](request & req,response & res){
            res.set_status_and_content(status_type::ok,"hello world,regex_route",req_content_type::string);
        });
        





    Server.run();
    return 0;
}
