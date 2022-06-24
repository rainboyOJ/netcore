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

    http_server hs(1,8899);
    hs.set_http_handler<GET>("/hello", [](request & req, response &res){
            res.set_status_and_content(status_type::ok, "hello world");
            });


    hs.regist_ws_conn_check("/ws", [](request & req,response & res){
            return true;
    });

    //websocket 功能
    // 处理过程 1.upgrade , 2 处理
    hs.set_http_handler<GET>("/ws",[](request & req,response & res){

            req.on(ws_open, [](request &req){
                    std::cout << "ws_open" << std::endl;
            });

            req.on(ws_close, [](request &req){
                    std::cout << "ws_close" << std::endl;
            });


            req.on(ws_message, [&res](request &req){
                    std::cout << "ws_message" << std::endl;
                    auto part_view = req.get_part_data();
                    std::cout << req.get_part_data() << std::endl;
                    //返回信息
                    //使用ws 创建一个信息
                    //req.get_conn()->build_ws_string(part_view.data(), part_view.length());
                    res.set_response(websocket::format_message(part_view.data(), part_view.length(), opcode::text));
            });

            req.on(ws_error, [](request &req){
                    std::cout << "ws_error" << std::endl;
            });

        });

    // url_query 处理
    // ?value=100
    hs.set_http_handler<GET>("/query",[](request & req,response & res){
            std::cout << "/query, the query is" << std::endl;
            int value = req.get_query_value<int>("value");
            std::cout << "value : " << value << std::endl;
            res.set_status_and_content(status_type::ok, std::to_string(value));
    });
    // 表单上传
    hs.set_http_handler<POST>("/multipart",[](request & req,response & res){
            std::cout << "/multipart, the query is" << std::endl;
            //int value = req.get_query_value<int>("value");
            auto value = req.get_multipart_value_by_key("value");
            std::cout << "value : " << value << std::endl;
            res.set_status_and_content(status_type::ok, std::move(value));
    });
    // 文件上传
    // 文件下载

    hs.run();

    return 0;
}
