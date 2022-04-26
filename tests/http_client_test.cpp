#include <iostream>
#include "http_client.hpp"
#include "client_factory.hpp"

int main(){
    auto hc = netcore::client_factory::instance().new_client();
    auto res = hc->get("http://neverssl.com");
    std::cout << res.resp_body << std::endl;
    return 0;
}
