#include "ws_before_ap.h"

using namespace netcore;
int main(){
    websocket_before_ap_mananger ws_ap_manger;
    // 注册
    ws_ap_manger.regist("/hello/world", [](request &,response &){
            return true;
    });
    // 执行
    response res;
    request r(res);
    bool ret = ws_ap_manger.invoke("/hello/world", r,res);
    std::cout << std::boolalpha << ret << std::endl;

    return 0;
}
