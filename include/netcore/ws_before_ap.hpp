/**
 * @desc 检查 是否可以进行websocket连接
 *  
 */

#pragma once

#include <functional>
#include <string_view>
#include <unordered_map>

#include "request.hpp"
#include "response.hpp"


namespace netcore {

class websocket_before_ap_mananger {
public:
    using AP_Type = std::function<bool(request &,response &)>;

    /**
     * @desc 注册
     */
    void regist(std::string_view name,AP_Type&& ap){
        ap_container.try_emplace(std::string(name), std::move(ap));
    }

    /**
     * @desc 调用相应的ap,不成功返回false
     */
    bool invoke(const std::string& url_name,request & req,response& res){
        auto func_iter = ap_container.find(url_name);
        if( func_iter != ap_container.end()) {
            return (func_iter->second)(req,res);
        }
        return false;
    }

private:
    std::unordered_map<std::string, AP_Type> ap_container;
};

} // end namespace netcore

