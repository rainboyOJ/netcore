//和judgeServer进行通信的client
#include <thread>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/fcntl.h>

#include "blockqueue.h"
#include "message.hpp"
#include "judgeServer.h"


using JUDGESEVER::judgeMessage;
using JUDGESEVER::resMessage;

namespace JUDGESEVER {

class judgeClient :public baseServer{

public:
    judgeClient(std::string_view serv_addr,short port)
    {
        memset(&servaddr,0,sizeof(servaddr));
        servaddr.sin_family=AF_INET;
        servaddr.sin_port=htons(port);
        servaddr.sin_addr.s_addr=inet_addr(serv_addr.data());
    }

    bool tryConnect(){
        if( isConnect ) return true;
        if ((sockfd=socket(AF_INET,SOCK_STREAM,0))<0) {
            LOG_ERROR("judgeClient socket() failed.\n");
            return isConnect = false;
        }

        if (connect(sockfd, (struct sockaddr *)&servaddr,sizeof(servaddr)) != 0)
        {
            LOG_ERROR("connect(%s:%d) failed.\n",inet_ntoa(servaddr.sin_addr),ntohs(servaddr.sin_port)); 
            close(sockfd);  
            return isConnect = false;
        }

        LOG_INFO("connect ok.\n");
        return isConnect = true;
    }

    ~judgeClient(){
        close(sockfd);
    }

    bool ping(){
        return baseServer::ping(sockfd);
    }

    /*virtual*/ void deal_pong(resMessage & rm){ LOG_INFO("recv pong"); }
    virtual void deal_judge_frame(resMessage & rm){

    }
    virtual void deal_compile_error(resMessage & rm){};
    virtual void deal_judge_error(resMessage & rm){};
    virtual void deal_judge_end(resMessage & rm){
        LOG_INFO("======== JUDGE END========")
    };

    //std::function<void(resMessage)> deal
    void dealRecv(); //处理接收的信息 的线程
    void dealSend(); //处理发收的信息 的线程
    void startRecvThread();
    void startSendThread();

    bool sendJudgeMessage(judgeMessage& jm){
        if( !isConnect ) return isConnect;
        if( !baseServer::sendMessag(sockfd, jm) )
            return isConnect = false;
        return true;
    }

    //发送judge信息
    // time memory pid uid lang code
    void sendJudge(int time,int memory, int pid, int uid,
            std::string_view lang, std::string_view code){
        //judgeMessage jm(time,memory,pid,uid,lang,code);
        que.push_back(std::make_shared<judgeMessage>(time,memory,pid,uid,lang,code));
        //return sendMessag(sockfd, jm);
    }

    void join(){
        if( read_thread.joinable()) read_thread.join();
        if( write_thread.joinable()) write_thread.join();
    }

private:
    int sockfd;
    std::thread read_thread;
    std::thread write_thread;

    std::atomic<bool> isConnect{false}; //是否和JudgeSever 连接
    std::atomic<int> cnt{0};

    struct sockaddr_in servaddr;
    const int MAX_CNT_NEED_PING{5}; //多少次没有数据之后发需要ping

    BlockDeque<judgeMessage::SHR_PTR> que; //评测队列
};

void judgeClient::dealSend(){
    //取出
    if( isConnect  == false) {
       if( !tryConnect() ){
           std::this_thread::sleep_for(std::chrono::seconds(1));
           return;
       }
    }

    std::shared_ptr<judgeMessage> jm;
    if( que.pop(jm,1) == true ){ //取成功
        if( !sendJudgeMessage(*jm) ) return;
    }
    else { //没有取到数据
        cnt.fetch_add(1);
        int max_cnt = MAX_CNT_NEED_PING;
        if( cnt.compare_exchange_strong(max_cnt,0) ) {
            LOG_DEBUG("ping");
            ping();//发送一个ping数据
        }
    }
}

void judgeClient::dealRecv(){
    //取出
    if( isConnect  == false) {
       if( !tryConnect() ){
           std::this_thread::sleep_for(std::chrono::seconds(1));
           return;
       }
    }

    // 处理接收的信息
    resMessage::SHR_PTR res = std::make_shared<resMessage>();
    
    int i;
    fd_set tmpfd;
    FD_ZERO(&tmpfd);
    FD_SET(sockfd,&tmpfd);

    if ( (i = select(sockfd+1,&tmpfd,0,0,0)) <= 0 ) return ;

    if( recvMessage(sockfd, *res) == false) {
        LOG_INFO("recvMessage fail");
        isConnect = false;
        return;
    }
    if( res->msg == "pong"){
        deal_pong(*res);
    }
    else if( res->status == judge::STATUS::PROBLEM_INFO){
    }
    else if( res->status == judge::STATUS::JUDGING){
    }
    else if( res->status == judge::STATUS::COMPILE_END ){
    }
    else if( res->status == judge::STATUS::COMPILE_ERROR){
    }
    else if( res->status == judge::STATUS::ERROR){
    }
    else if( res->status == judge::STATUS::END){
        deal_judge_end(*res);
    }

    //res->debug(); //TODO
}


void judgeClient::startRecvThread() {
        read_thread = std::thread([this](){
            while ( 1 ) {
                this->dealRecv();
            }
        });
}

void judgeClient::startSendThread() {
        write_thread = std::thread([this](){
            while ( 1 ) {
                this->dealSend();
            }
        });
}

} // namespace JUDGESEVER end
