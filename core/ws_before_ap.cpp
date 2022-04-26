#include "ws_before_ap.h"

namespace netcore {
    
void websocket_before_ap_mananger::regist(std::string_view name, AP_Type &&ap){
    ap_container.try_emplace(std::string(name), std::move(ap));
}

bool websocket_before_ap_mananger::invoke(const std::string &url_name, request &req, response &res){
    auto func_iter = ap_container.find(url_name);
    if( func_iter != ap_container.end()) {
        return (func_iter->second)(req,res);
    }
    return false;
}

} // end namespace netcore

