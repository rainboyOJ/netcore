#include "ws_before_ap.h"

namespace rojcpp {
    
void websocket_before_ap_mananger::regist(std::string_view name, AP_Type &&ap){
    ap_container.try_emplace(std::string(name), std::move(ap));
}

bool websocket_before_ap_mananger::invoke(const std::string &url_name, request &req, response &res){
    if( ap_container.count(url_name) != 0) {
        return true;
    }
    return false;
}

} // end namespace rojcpp

