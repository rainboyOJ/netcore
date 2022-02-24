//websocket 管理
// @desc 实现websocket连接的管理,注册后可以通过多线程的发送信息
//      
#pragma once
#include <string>
#include <set>
#include <string_view>
#include <mutex>
#include <unordered_map>

#include<sys/socket.h>

#include "define.h"
#include "../lib/threadpool.hpp" // 线程池
#include "websocket.hpp"


namespace rojcpp {

class WS_manager {
public:
    static WS_manager& get_instance(){
        static WS_manager ws_manager;
        return ws_manager;
    }

    /**
     * @brief 注册
     * id 字符串,代表评测id,只能注册一个
     * fd 
     */
    bool regist(std::string id,int fd);
    /**
     * @brief 反注册
     * 在ws关闭的时候 反注册
     */
    bool unregist(std::string id,int fd);
    //是否存活,是否存在
    bool is_alive(std::string_view id);

    //发送信息
    //fd 代表
    //1. 要发送的socket的 fd
    //2. connection 存在map里的下标
    //所有可以通过fd来获取connection
    void send_message(int fd,std::string&& msg){
        //hsp->get_conn()->send_ws_string(std::move(msg));
    }

    //template<typename... Fs>
    void send_ws_string(int fd,std::string msg,bool close = false) {
        //send_ws_msg(fd,std::move(msg), opcode::text, std::forward<Fs>(fs)...);
        send_ws_msg(fd,std::move(msg), opcode::text,close);
    }

    //关闭,发送关闭的信息
    static void Close(int fd){
        shutdown(fd, SHUT_RDWR); //发送关闭的信息
    }

private:
    //template<typename... Fs>
    void send_ws_msg(int fd,std::string msg, opcode op = opcode::text ,bool close = false) {
        //constexpr const size_t size = sizeof...(Fs);
        //static_assert(size == 0 || size == 2);
        //if constexpr(size == 2) {
            //set_callback(std::forward<Fs>(fs)...); // 执行call back
        //}
        auto header = websocket::format_header(msg.length(), op);
        //send_msg(std::move(header), std::move(msg));
        //std::string send_msg = header + msg;
        thpool.commit([send_msg = header + msg,close,fd,this](){
            std::lock_guard<std::mutex> lock(fd_mutex[fd % 4]);
            Writen(fd, send_msg.c_str() , send_msg.length());
            if( close )
                Close(fd);
        });
    }

    // TODO 因为的 connection 里的同名函数 重复了 ,所以应该放到另一个文件里
    //返回写入的长度，<=0 是错误
    int Writen(int fd_,const char *buffer,const size_t n)
    {
        int nLeft,idx,nwritten;
        nLeft = n;  
        idx = 0;
        while(nLeft > 0 )
        {
            if ( (nwritten = send(fd_, buffer + idx,nLeft,0)) <= 0) 
                return nwritten;
            nLeft -= nwritten;
            idx += nwritten;
        }
        return n;
    }


    WS_manager() = default;
    THREAD_POOL::threadpool thpool{__config__::ws_thpool_size};
    std::mutex m_mutex;
    std::unordered_map<std::string, int> u_map;
    std::array<std::mutex, 4> fd_mutex; //保证只有一个线程在写同一个fd
    std::set<int> fd_set;
};

} // namespace rojcpp end
