//和judgeServer进行通信的client
//
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/fcntl.h>

#include "message.hpp"
#include "judgeServer.h"

using JUDGESEVER::judgeMessage;
using JUDGESEVER::resMessage;

namespace JUDGESEVER {

class judgeClient :public baseServer{

public:
    judgeClient(std::string_view serv_addr,short port)
        :port{port}
    {
        if ((sockfd=socket(AF_INET,SOCK_STREAM,0))<0) {
            printf("socket() failed.\n");
            return;
        }
        struct sockaddr_in servaddr;
        memset(&servaddr,0,sizeof(servaddr));
        servaddr.sin_family=AF_INET;
        servaddr.sin_port=htons(port);
        servaddr.sin_addr.s_addr=inet_addr(serv_addr.data());

        if (connect(sockfd, (struct sockaddr *)&servaddr,sizeof(servaddr)) != 0)
        {
            printf("connect(%s:%d) failed.\n",serv_addr.data(),port); 
            close(sockfd);  
            return ;
        }

        isConnect = true;
        printf("connect ok.\n");
    }

    ~judgeClient(){
        close(sockfd);
    }

    bool ping(){
        return baseServer::ping(sockfd);
    }

    //std::function<void(resMessage)> deal
    bool dealRecv(){
        resMessage res;
        if( recvMessage(sockfd, res) == false) {
            log("recvMessage fail");
            return false;
        }
        res.debug();
        return 1;
    }

    //发送judge信息
    // time memory pid uid lang code
    bool sendJudge(int time,int memory, int pid, int uid,
            std::string_view lang, std::string_view code){
        judgeMessage jm(time,memory,pid,uid,lang,code);
        return sendMessag(sockfd, jm);
    }

private:
    bool isConnect{false}; //是否和JudgeSever 连接
    short port;
    int sockfd;
};

}
