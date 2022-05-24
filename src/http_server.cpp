#include "http_server.h"

namespace netcore {

    Task Server::handle(Connection::CONN_PTR conn) 
    {
        for(;;) {
            try {
                char buff[100];
                auto nbtyes = co_await conn->async_read(buff,sizeof(buff)-1,3s);
                // 进行相应的处理
                if( nbtyes == 0)
                    break;
                log("server :[",m_id,"]recv bytes:",nbtyes);
                log("recv content:",std::string(buff));
                //log("sleep 10...");
                log("1,ready to send nbytes:",nbtyes);
                std::this_thread::sleep_for(std::chrono::seconds(3));
                log("2,ready to send nbytes:",nbtyes);

                nbtyes = co_await conn->async_send(buff, nbtyes);

                if( nbtyes == 0){
                    log("======> send nbytes 0");
                    break;
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

