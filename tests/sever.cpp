#include <regex>
#include "http_server.hpp"
#include "websocket_manager.h"

using namespace rojcpp;
struct check_login {
    bool before(request& req,response& rep){
        //auto weak_session = req.get_session();
        //if( auto session = weak_session.lock(); session != nullptr){
            //auto isLogin = session->get_data<bool>("isLogin");
            //if( isLogin ) return true;
        //}
        //rep.set_status_and_content(status_type::ok,"your are not logined!",req_content_type::string);
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
                //auto weak_session = req.get_session();
                //if( auto session = weak_session.lock(); session != nullptr){
                    //auto isLogin = session->get_data<bool>("isLogin");
                    //if( isLogin ) {
                        //res.set_status_and_content(status_type::ok,"you have logined!",req_content_type::string);
                        //return ;
                    //}
                //}
                //auto session = res.start_session();
                //session->set_data("isLogin", true);
                res.set_status_and_content(status_type::ok,"yes,now logined",req_content_type::string);
            });

    // string -> body  eacho
    Server.set_http_handler<POST>("/string_body_echo",
            [](request & req,response & res){
            auto res_type = req.get_content_type() == content_type::json 
                            ? req_content_type::json
                            : req_content_type::string;
                res.set_status_and_content(status_type::ok,
                        std::string(req.body())
                        ,res_type);
            });


    Server.set_http_handler<POST>("/signout",
            [](request & req,response & res){
                //auto weak_session = req.get_session();
                //if( auto session = weak_session.lock(); session != nullptr){
                    //auto isLogin = session->get_data<bool>("isLogin");
                    //if( isLogin ) {
                        //res.set_status_and_content(status_type::ok,"you have logined!",req_content_type::string);
                        //return ;
                    //}
                //}
                //auto session = res.start_session();
                //session->set_data("isLogin", true);
                res.set_status_and_content(status_type::ok,"yes,now logined",req_content_type::string);
            });

    //上传文件
    Server.set_http_handler<POST>("/upload", [](request& req,response& res){
                res.set_status_and_content(status_type::ok,"yes,upload success!",req_content_type::string);
            });


    Server.regist_ws_conn_check("/ws", [](request& req,response & res) -> bool{
        if( req.get_url() == "/ws"sv) {
            return true;
        }
        return false;
    });

    //websocket 测试
    Server.set_http_handler<GET, POST>("/ws", [](request& req, response& res) {
        //assert(req.get_content_type() == content_type::websocket);

        req.on(ws_open, [](request& req){
            std::cout << "websocket start" << std::endl;
        });

        req.on(ws_message, [](request& req) {
            std::cout << "ws_message :test" << std::endl;
            auto part_data = req.get_part_data();
            //echo
            std::string str = std::string(part_data.data(), part_data.length());
            //req.get_conn()->send_ws_string(std::move(str));
            WS_manager::get_instance().send_ws_string(req.get_conn()->GetFd(),std::move(str));
            std::cout << part_data.data() << std::endl;
        });

        //req.on(ws_error, [](request& req) {
            //std::cout << "websocket pack error or network error" << std::endl;
        //});
    });




    Server.run();
    return 0;
}
