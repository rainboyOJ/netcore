#include <iostream>
#include "io_context.h"
#include "task.h"
#include "acceptor.h"


// 创建一个server
using namespace netcore;

Task listen(IoContext & ctx){
    log(__FILE__,__LINE__,"in listen");
    Acceptor acceptor(ctx,Protocol::ip_v4(),Endpoint(Address::Any(),8899)); //完成一些初始化操作
    for(;;){
        Connection conn = co_await acceptor.async_accept(); //进入 AcceptorAwaiter
        std::cout << "conn ok" << std::endl;
    }
}

int main(){

    //创建一context
    IoContext ioctx;

    co_spawn(listen(ioctx));

    ioctx.run();




    return 0;
}
