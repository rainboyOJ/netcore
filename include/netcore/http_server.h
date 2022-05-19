//创建一个Server,支持多线程

#pragma once

#include <thread>


#include "io_context.h"
#include "acceptor.h"
#include "connection.h"


namespace netcore {

class http_server;

class Server {
    friend http_server;
    private:
        IoContext m_ctx;
        std::thread m_thread;
        Acceptor m_acceptor;
        int m_id;
    public:
        Server()  = delete;
        Server(int id,Acceptor & acc);
        Server(int id,NativeHandle epollfd,Acceptor & acc);
        Task handle(Connection::CONN_PTR conn) {
            for(;;) {
                char buff[100];
                auto nbtyes = co_await conn->async_read(buff,sizeof(buff)-1);
                // 进行相应的处理
                if( nbtyes == 0)
                    break;
                log("server :[",m_id,"]recv bytes:",nbtyes);
                log("recv content:",std::string(buff));
                //log("sleep 10...");
                std::this_thread::sleep_for(std::chrono::seconds(3));

                nbtyes = co_await conn->async_send(buff, nbtyes);

                if( nbtyes == 0)
                    break;
            }
        }
        Task listen() //启动
        {
            for(;;) {
                log("async_accept at thread : ",m_id);
                auto conn = co_await m_acceptor.async_accept();
                co_spawn(handle(std::move(conn)));
            }
        }
        void Serve();  //运行
};
    
//============================================

class http_server {

private:
    std::size_t m_num; //线程的数量
    std::vector<Server> m_servers;
    unsigned int m_port;
    Acceptor m_acceptor;
public:
    http_server() = delete;
    http_server(std::size_t num,unsigned int port);
    ~http_server();
    void run(); //运行
};

} // end namespace netcore

