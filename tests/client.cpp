#include <iostream>
#include "http_client.hpp"
#include "client_factory.hpp"

using namespace std;

const std::string Server  = "http://localhost:8099";

void test_hello_world(){
    auto hc = rojcpp::client_factory::instance().new_client();
    auto res = hc->get(Server + "/helloworld");
    std::cout << res.resp_body << std::endl;
    auto [header,size] =  res.resp_headers ;
    std::cout << "headers: " << std::endl;
    for(int i=0;i<=size-1;++i){
        std::cout << std::string_view(header[i].name,header[i].name_len) << " : " ;
        std::cout << std::string_view(header[i].value,header[i].value_len) << " \n" ;
    }
}

int main(){
    test_hello_world();

    return 0;
}
