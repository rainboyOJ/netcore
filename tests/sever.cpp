#include <regex>
#include "http_server.hpp"

using namespace rojcpp;
struct check_login {
    bool before(request& req,response& rep){
        auto weak_session = req.get_session();
        if( auto session = weak_session.lock(); session != nullptr){
            auto isLogin = session->get_data<bool>("isLogin");
            if( isLogin ) return true;
        }
        rep.set_status_and_content(status_type::ok,"your are not logined!",req_content_type::string);
        return false;
    }
};

//创建一个全局的服务器
rojcpp::http_server_ Server;
int main(){
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
    
    Server.set_http_handler<GET>("/msg_after_logined",
            [](request & req,response & res){
                res.set_status_and_content(status_type::ok,"yes! you logined! This secret msg!",req_content_type::string);
            } , check_login{});

    Server.set_http_handler<POST>("/login",
            [](request & req,response & res){
                auto weak_session = req.get_session();
                if( auto session = weak_session.lock(); session != nullptr){
                    auto isLogin = session->get_data<bool>("isLogin");
                    if( isLogin ) {
                        res.set_status_and_content(status_type::ok,"you have logined!",req_content_type::string);
                        return ;
                    }
                }
                auto session = res.start_session();
                session->set_data("isLogin", true);
                res.set_status_and_content(status_type::ok,"yes,now logined",req_content_type::string);
            });

    Server.set_http_handler<POST>("/signout",
            [](request & req,response & res){
                auto weak_session = req.get_session();
                if( auto session = weak_session.lock(); session != nullptr){
                    auto isLogin = session->get_data<bool>("isLogin");
                    if( isLogin ) {
                        res.set_status_and_content(status_type::ok,"you have logined!",req_content_type::string);
                        return ;
                    }
                }
                auto session = res.start_session();
                session->set_data("isLogin", true);
                res.set_status_and_content(status_type::ok,"yes,now logined",req_content_type::string);
            });

    //上传文件
    Server.set_http_handler<POST>("/upload", [](request& req,response& res){
                res.set_status_and_content(status_type::ok,"yes,upload success!",req_content_type::string);
            });




    Server.run();
    return 0;
}
