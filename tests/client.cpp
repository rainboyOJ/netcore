#include <iostream>
#include "http_client.hpp"
#include "client_factory.hpp"
#include "testlib.hpp"

using namespace std;

const std::string Server  = "http://localhost:8099";

void test_hello_world(){
    auto hc = netcore::client_factory::instance().new_client();
    auto res = hc->get(Server + "/helloworld");
    std::cout << res.resp_body << std::endl;
    ok( res.resp_body == "hello world,this netcore Server","body ok");
    auto [header,size] =  res.resp_headers ;
    std::cout << "headers: " << std::endl;
    for(int i=0;i<=size-1;++i){
        std::cout << std::string_view(header[i].name,header[i].name_len) << " : " ;
        std::cout << std::string_view(header[i].value,header[i].value_len) << " \n" ;
    }
}

void test_regex_route(){
    for(int i=1;i<=5;++i){
        auto hc = netcore::client_factory::instance().new_client();
        std::string url = Server + "/regex/hello/1";
        auto res = hc->get(url);
        std::string msg = std::to_string(i) + " regex body ok";
        ok(res.resp_body == "hello world,regex_route",msg.c_str());
    }
}

void test_upload_file(){
    auto hc = netcore::client_factory::instance().new_client();
    //auto res = 
    //hc->upload_string(Server+"/upload", "test.cpp",10,nullptr);
    hc->upload_string(Server+"/upload", "test.cpp",5_MB,nullptr); //5MB
}

int main(){
    //test_hello_world();
    //test_regex_route();
    test_upload_file();

    return 0;
}
