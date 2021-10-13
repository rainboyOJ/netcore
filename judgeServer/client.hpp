//和judgeServer进行通信的client
//
#include "message.hpp"
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/fcntl.h>


class judgeClient {
private:
    void ping(); //心跳检测连接
    bool isConnect; //是否和JudgeSever 连接
};
