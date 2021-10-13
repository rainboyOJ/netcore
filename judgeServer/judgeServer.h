#pragma once
#include <iostream>

#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/fcntl.h>

#include "message.hpp"

class JudgeSever {

public:
    int  start(); //启动
    bool send(); //发送评测的结果
    bool pong(); //心跳回应
    void judge();

private:
    int initserver();
    int    port;   //端口
    int    listensock; //监听的socket
    int    maxfd;  // readfdset中socket的最大值。
    fd_set readfdset;  // 读事件的集合，包括监听socket和客户端连接上来的socket。

};
