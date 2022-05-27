//创建一个Server,支持多线程

#pragma once

#include <thread>
#include "core.h"
#include "logWrapper.hpp"

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

        //处理发生connection
        Task handle(Connection::CONN_PTR conn) ;

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

