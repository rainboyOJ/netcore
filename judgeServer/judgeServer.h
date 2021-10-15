#pragma once
#include <iostream>

#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/fcntl.h>

#include "message.hpp"
#include "uuid.hpp"
#include "judgeWork.hpp"
#include "log.h"

namespace JUDGESEVER {

using JUDGESEVER::judgeMessage;
using JUDGESEVER::resMessage;
#define bufsiz 1024

class baseServer{
private:
protected:
    //发送的评测/结果信息
    template<typename T>
    bool sendMessag(int sockfd,T &m){
        auto mstr = m.encode();
        //log("needSendSize",mstr.length());
        int mlen = htonl(mstr.length());
        std::string ss;;
        ss.resize(4);
        memcpy(ss.data(), &mlen , sizeof(mlen));
        ss.append(mstr);
        //for (const auto& e : ss) {
            //std::cout << std::hex << int(e) << " ";
        //}
        //std::cout  << std::endl;
        return Writen(sockfd, ss) == ss.length();
    }

    //接收 评测/结果 信息
    template<typename T>
    bool recvMessage(int sockfd,T &m){
        char buf[bufsiz];
        if( Readn(sockfd,4,buf) != 4) return false;
        int needReadSize = ntohl(*reinterpret_cast<int *>(buf));
        //log_one(needReadSize);
        while (needReadSize > 0) {
            int readn = Readn(sockfd,needReadSize,buf);
            //log_one(readn);
            if(  readn ==-1 ) return false;
            for(int i=0;i<readn;++i){
                m.decode(buf[i]); // TODO last readStatus
            }
            needReadSize -= readn;
        }
        //std::cout  << std::endl;
        return true;
    }

    bool ping(int sockfd){
        judgeMessage jm(0,0,0,0,"ping","ping");
        return sendMessag(sockfd , jm);
    }

    bool pong(int sockfd){
        resMessage rm;
        rm.socket = sockfd;
        rm.status = judge::STATUS::END;
        rm.msg = "pong";
        return sendMessag(sockfd, rm);
    }

private:

    //返回写了多少字节
    int Writen(int sockfd,std::string_view msg){ //保证发送完全
        int nwritten;
        auto nLeft = msg.length();
        auto total = nLeft;
        while ( nLeft > 0 ) {
            if( (nwritten = write(sockfd, msg.data(), msg.size()) ) <=0 ) 
                return -1;
            nLeft -= nwritten;
            msg.remove_prefix(nwritten);
        }
        return total;
    }

    //读取buffer 满为止
    //返回的是读取了多个字符
    int Readn(int sockfd,int n,char *buf)
    {
        int nLeft,total,nread,idx;
        total = nLeft = std::min(n,bufsiz);
        //log_one(total);
        idx = 0;

        while(nLeft > 0)
        {
            if ( (nread = read(sockfd,buf+ idx,nLeft)) <= 0) return -1;
            //for(int i=0;i<nread;++i){
               //std::cout << std::hex << int(buf[i]) << " ";
            //}
            //std::cout  << std::endl;

            idx += nread;
            nLeft -= nread;
        }

        return total;
    }
};

class JudgeServer :public baseServer {

public:
    using resType = std::shared_ptr<resMessage>;
    JudgeServer(short port, std::string_view problem_base,std::string_view judge_base_path)
        : port{port} , jw{this,4,problem_base,judge_base_path}
    {}
    int  start(); //启动
    bool send(); //发送评测的结果
    void judge();
    void addResMessage(resType res){
        sendResThpool.commit([res,this](){
                this->sendMessag(res->socket, *res);
        });
    }

private:
    int initserver();
    short    port;   //端口
    int    listensock; //监听的socket
    int    maxfd;  // readfdset中socket的最大值。
    fd_set readfdset;  // 读事件的集合，包括监听socket和客户端连接上来的socket。

    UUID<int> uuid;
    judgeWork<JudgeServer> jw;
    THREAD_POOL::threadpool sendResThpool{1}; //单个线程

};

} //namespace JudgeSever
