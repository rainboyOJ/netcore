//创建一个Server,支持多线程

#pragma once

#include <thread>
#include "core.hpp"
#include "connection.hpp"

namespace netcore {

class http_server;

class Server {
    friend http_server;
    private:
        IoContext m_ctx;
        std::thread m_thread;
        Acceptor m_acceptor;
        std::string static_dir = "www";
        int m_id;
    public:
        Server()  = delete;
        Server(int id,Acceptor & acc)
            :m_id(id)
        {
            m_acceptor.reset_io_context(m_ctx, acc);
        }
        Server(int id,NativeHandle epollfd,Acceptor & acc)
            :m_id(id),m_ctx(epollfd)
        {
            m_acceptor.reset_io_context(m_ctx, acc);
        }


        Task listen() //启动
        {
            for(;;) {
                log("async_accept at thread : ",m_id);
                auto conn = co_await m_acceptor.async_accept();
                co_spawn(handle(std::move(conn)));
            }
        }
        //运行
        void Serve(){
            m_thread = std::thread([this]{
                    try {
                    log("$Server start id : $",m_id);
                    co_spawn(listen());
                    log("Server run at server id : ",m_id);
                    m_ctx.run();
                    }
                    catch(const std::exception &e){
                    //log(std::current_exception());
                    log("catch, error 1");
                    log(e.what());
                    }
                    //catch(...){
                    //log("catch, error 2");
                    //}
                    });
        }

        //处理发生connection
        Task handle(RawConnection::CONN_PTR conn) 
        {
            for(;;) {
                try {
                    bool break_for_flag = false;

                    auto conn_handle = std::make_shared<connection>(
                            3*1024*1024, //max_req_size 3MB
                            10*60, // keep_alive_timeout
                            nullptr,      //http_handler  的函数指针
                            nullptr, // http_handler_check 的函数指针
                            nullptr,// ws_connection_check * ws_conn_check,
                            static_dir,    //静态资源目录
                            nullptr,//upload_check_handler * upload_check //上传查询
                            conn->socket()
                            );

                    do  {

                        auto nbtyes = co_await conn->async_read(conn_handle->req().buffer(),conn_handle->req().left_size(),100s);
                        // 进行相应的处理
                        if( nbtyes == 0){
                            break_for_flag = true;
                            break ;
                        }

                        auto pc_state = conn_handle->process();
                        if( pc_state == process_state::need_read)
                            continue;
                    } while(0);

                    if( break_for_flag ) break;


                    auto send_bufs = conn_handle->res().to_buffers();
                    for (auto& e : send_bufs) {
                        auto nbtyes = co_await conn->async_send(e.data(),e.length(),100s);
                        if( nbtyes == 0){
                            log("======> send nbytes 0");
                            break;
                        }
                    }


                }
                catch(const netcore::SendError& e){
                    log(e.what());
                    conn->clear_except();
                    break;
                }
                catch(const netcore::RecvError& e){
                    log(e.what());
                    conn->clear_except();
                    break;
                }
                catch(const netcore::IoTimeOut& e){
                    log(e.what());
                    conn->clear_except();
                    break;
                }

            } // end for(;;)
        }
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
    http_server(std::size_t num,unsigned int port) 
        :m_port(port),m_num(num),
        m_acceptor(Protocol::ip_v4(),Endpoint(Address::Any(),port))
    {
        //::accept(int __fd, struct sockaddr *__restrict __addr, socklen_t *__restrict __addr_len)
        m_servers.reserve(num);
        for(int i=0;i<num;++i){
            m_servers.emplace_back(i,m_acceptor);
        }
    }

    ~http_server() {
        for(int i=0;i<m_num;++i){
            if(m_servers[i].m_thread.joinable())
                m_servers[i].m_thread.join();
        }
    }
    void run() //运行
    {
        for(int i=0;i<m_num;++i){
            m_servers[i].Serve();
        }
    }
};

} // end namespace netcore

