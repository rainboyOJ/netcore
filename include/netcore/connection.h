/**
 * connection : 对连接的封装
 *
 * 核心功能 : 
 *  async_read 异步读 ret > 0 返回读取的数据
 *  async_send 异步写
 *  close 关闭
 *   - 关闭所有的awaiter
 *   - ::close 调用时,会自动从epoll里移除
 *   - 对端和本端对 读写的关闭(暂时不支持,我觉得不需要,只要read,send 有异常,就关闭connection)
 *    参考[epoll触发事件的分析_halfclear的博客-CSDN博客](https://blog.csdn.net/halfclear/article/details/78061771)
 *
 */
#pragma once
#include "basic.hpp"
#include "io_context.h"
#include "task.h"

namespace netcore {

    long long recv(NativeSocket sock,char *buf,std::size_t buff_size);
    //发送数据,返回发送数据大小
    long long send(NativeHandle sock,char *buf,long long send_size);


    struct Connection;

// ================ callback

    struct ConnCallBack : public CallbackImplBase{
        Connection * m_connection;

        ConnCallBack(Connection * conn) 
            : CallbackImplBase(this), m_connection{conn}
        {}
        void on_callback(IoEvent& evt);
    };

// ================ awaiter
    struct AsyncIoWaiterBases {
        ListNode  m_node;
        Connection * m_conn{nullptr};
        std::size_t m_tranfer_bytes{0};
        char * m_buff;
        std::size_t m_buff_size;
        std::coroutine_handle<TaskPromise> m_h;

        AsyncIoWaiterBases(Connection * conn,char * buff,std::size_t buff_size) 
            : m_conn{conn},m_buff{buff},m_buff_size{buff_size}
        {}

        static AsyncIoWaiterBases *from_node(ListNode *node) {
            return (AsyncIoWaiterBases*)((char*)node - offsetof(AsyncIoWaiterBases, m_node));
        }
        bool await_ready() { return false; }

        //template<class Promise>
        //inline void await_suspend(std::coroutine_handle<Promise> suspend_coroutine) {
            //auto h = suspend_coroutine.promise().coroutine_handle();
            //await_suspend(h);
        //}

        void unregister(NativeSocket conn_handle);
    };

    struct AsyncReadAwaiter :public AsyncIoWaiterBases {
        using AsyncIoWaiterBases::AsyncIoWaiterBases;
        bool await_suspend(std::coroutine_handle<TaskPromise> h );
        std::size_t await_resume();
    };

    struct AsyncSendAwaiter :public AsyncIoWaiterBases {

        char * buf(); //发送数据的位置
        std::size_t left_buf_size();
        void update_sent_size(std::size_t siz); //更新已经发送的数据大小
        bool finised(); //数据是否发送完毕

        using AsyncIoWaiterBases::AsyncIoWaiterBases;
        bool await_suspend(std::coroutine_handle<TaskPromise> h );
        std::size_t await_resume();
    };

// ================ Connection

    class Connection {
            friend AsyncReadAwaiter;
            friend AsyncSendAwaiter;
            friend ConnCallBack;
        private:
            NativeSocket m_socket{-1};
            IoContext * m_ctx{nullptr};

            //bool m_recv_shutdown = false;
            //bool m_send_shutdown = false;
            //bool m_tcp_nodelay   = false;

            bool m_added_to_event_pool = false;

            AsyncReadAwaiter * m_read_awaiter{nullptr};
            AsyncSendAwaiter * m_send_awaiter{nullptr};
            ConnCallBack m_callback{this};

            std::exception_ptr m_except{nullptr}; //存异常

        public:
            using CONN_PTR = std::unique_ptr<Connection>;
            
            bool m_send_at_awaiter = false; // 是否需要在awaiter 里写
            bool m_recv_at_awaiter = false; // 是否需要在awaiter 里写

            Connection(IoContext * _ctx ,NativeHandle socket_fd)
                :m_ctx{_ctx},m_socket{socket_fd}
            {
                registerToIoCtx();
            }

            Connection(Connection &&);
            void operator=(Connection &&);

            Connection(Connection const &) = delete;
            void operator=(Connection const &) = delete;

            void clear_except();

            ~Connection();

            /**
             * !!注意 async_read和async_send 都不支持在多个同时在多个携程内调用
             */
            // retvalue=0 表示连接断开了
            AsyncReadAwaiter async_read(char * buf,std::size_t buff_size);
            // retvalue=0 表示连接断开了
            AsyncSendAwaiter async_send(char * buf,std::size_t buff_size);
            void close();
            //static void on_callback(IoEvent & evt);

        private:
            void registerToIoCtx();
            void unRegisterToIoCtx();
    };
    
} // end namespace netcore

