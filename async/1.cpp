//echo server
#include <iostream>
#include "io_context.h"
#include "task.h"
#include "acceptor.h"


// 创建一个server
using namespace netcore;

Task handle_conn(Connection::CONN_PTR conn){
    log(__FILE__,__LINE__,"in handle_conn");
    for(;;) {
        char buf[100];
        std::cout << "await message ...." << std::endl;
        auto nread = co_await conn->async_read(buf, 99);
        if( nread == 0)
            break;

        auto nwrite = co_await conn->async_send(buf, nread);
        if( nwrite == 0)
            break;
    }

}

Task listen(IoContext & ctx){
    log(__FILE__,__LINE__,"in listen");
    Acceptor acceptor(ctx,Protocol::ip_v4(),Endpoint(Address::Any(),8899)); //完成一些初始化操作
    for(;;){
        log( "await acceptor.....");
        auto conn_ptr = co_await acceptor.async_accept(); //进入 AcceptorAwaiter
        std::cout << "conn ok" << std::endl;
        co_spawn(handle_conn(std::move(conn_ptr)));
    }
}

int main(){

    //创建一context
    IoContext ioctx;

    try {
        co_spawn(listen(ioctx));

        ioctx.run();
    }
    catch(...){
        log(__FILE__,__LINE__,"some error");
        return -1;
    }


    return 0;
}
