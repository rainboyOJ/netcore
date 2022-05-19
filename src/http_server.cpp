#include "http_server.h"

namespace netcore {

    Server::Server(int id,Acceptor & acc)
        :m_id(id)
    {
        m_acceptor.reset_io_context(m_ctx, acc);
    }

    Server::Server(int id,NativeHandle epollfd,Acceptor & acc)
        :m_id(id),m_ctx(epollfd)
    {
        m_acceptor.reset_io_context(m_ctx, acc);
    }

    void
    Server::Serve() {
        m_thread = std::thread([this]{
                try {
                    log("Server start id : ",m_id);
                    co_spawn(listen());
                    log("Server run at server id : ",m_id);
                    m_ctx.run();
                }
                catch(std::exception &e){
                    //log(std::current_exception());
                    log(e.what());
                }
            });
    }


    http_server::http_server(std::size_t num,unsigned int port)
        :m_port(port),m_num(num),
        m_acceptor(Protocol::ip_v4(),Endpoint(Address::Any(),port))
    {
        //::accept(int __fd, struct sockaddr *__restrict __addr, socklen_t *__restrict __addr_len)
        m_servers.reserve(num);
        for(int i=0;i<num;++i){
            m_servers.emplace_back(i,m_acceptor);
        }
    }
    http_server::~http_server(){
        for(int i=0;i<m_num;++i){
            if(m_servers[i].m_thread.joinable())
                m_servers[i].m_thread.join();
        }
    }

    void http_server::run() {
        for(int i=0;i<m_num;++i){
            m_servers[i].Serve();
        }
    }


} // end namespace netcore

