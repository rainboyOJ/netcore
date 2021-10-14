#include "judgeServer.h"
namespace JUDGESEVER {

int JudgeServer::start(){

    // 初始化服务端用于监听的socket。
    listensock = initserver();
    log("建立server 成功");

    if (listensock < 0)
    {
        log("initserver() failed.\n"); 
        return -1;
    }


    // 初始化结构体，把listensock添加到集合中。
    FD_ZERO(&readfdset);
    FD_SET(listensock,&readfdset);
    maxfd = listensock;

    while (1)
    {
        // 调用select函数时，会改变socket集合的内容，所以要把socket集合保存下来，传一个临时的给select。
        fd_set tmpfdset = readfdset;

        int infds = select(maxfd+1,&tmpfdset,NULL,NULL,NULL);
        // printf("select infds=%d\n",infds);

        // 返回失败。
        if (infds < 0)
        {
            log("select() failed.\n"); perror("select()"); break;
        }

        // 超时，在本程序中，select函数最后一个参数为空，不存在超时的情况，但以下代码还是留着。
        if (infds == 0)
        {
            log("select() timeout.\n"); continue;
        }

        // 检查有事情发生的socket，包括监听和客户端连接的socket。
        // 这里是客户端的socket事件，每次都要遍历整个集合，因为可能有多个socket有事件。
        for (int eventfd=0; eventfd <= maxfd; eventfd++)
        {
            if (FD_ISSET(eventfd,&tmpfdset)<=0) continue;

            if (eventfd==listensock)
            { 
                // 如果发生事件的是listensock，表示有新的客户端连上来。
                struct sockaddr_in client;
                socklen_t len = sizeof(client);
                int clientsock = accept(listensock,(struct sockaddr*)&client,&len);
                if (clientsock < 0)
                {
                    log("accept() failed.\n"); continue;
                }

                log("client(socket=%d) connected ok.\n",clientsock);

                // 把新的客户端socket加入集合。
                FD_SET(clientsock,&readfdset);

                if (maxfd < clientsock) maxfd = clientsock;
                continue;
            }
            else
            {
                // 客户端有数据过来或客户端的socket连接被断开。
                log("recv msg");
                judgeMessage jm;
                jm.uid = uuid.get();
                jm.socket = eventfd;
                if( recvMessage(eventfd,jm) == false) {
                    log("client(eventfd=%d) disconnected.\n",eventfd);
                    close(eventfd);  // 关闭客户端的socket。
                    FD_CLR(eventfd,&readfdset);  // 从集合中移去客户端的socket。
                    // 重新计算maxfd的值，注意，只有当eventfd==maxfd时才需要计算。
                    if (eventfd == maxfd)
                    {
                        for (int ii=maxfd;ii>0;ii--)
                        {
                            if (FD_ISSET(ii,&readfdset))
                            {
                                maxfd = ii; break;
                            }
                        }
                    }
                    continue;
                }
                log("recv msg end");
                if( jm.lang == "ping"){ //用户发过的ping
                    log("recv ping");
                    pong(eventfd);
                }
                else { //加入judgeWork的队列
                    log("加入judgeWork的队列");
                    jw.add(jm);
                }
                // 把收到的报文发回给客户端。
                //write(eventfd,buffer,strlen(buffer));
            }
        }
    }
}; 

int JudgeServer::initserver()
{
    int sock = socket(AF_INET,SOCK_STREAM,0);
    if (sock < 0)
    {
        log("socket() failed.\n");
        return -1;
    }

    // Linux如下
    int opt = 1; unsigned int len = sizeof(opt);
    setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&opt,len); //重用
    setsockopt(sock,SOL_SOCKET,SO_KEEPALIVE,&opt,len); //长连接

    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    if (bind(sock,(struct sockaddr *)&servaddr,sizeof(servaddr)) < 0 )
    {
        log("bind() failed.\n"); close(sock); return -1;
    }

    if (listen(sock,5) != 0 )
    {
        log("listen() failed.\n");
        close(sock);
        return -1;
    }

    return sock;
}

} //namespace JudgeSever
