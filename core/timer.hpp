#pragma once
#include <unistd.h>
#include <signal.h>
#include <functional>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>

#include <cstring>
#include <cassert>

#include "log.h"

namespace rojcpp {

class Timer;


inline void sig_handler_wrapper(int);
//定时器
class Timer {
public:
    static Timer* getInstance(){
        static Timer timer_;
        return &timer_;
    }
    Timer(){
        int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
        //set no block
        int old_option = fcntl(m_pipefd[0], F_GETFL);
        int new_option = old_option | O_NONBLOCK;
        fcntl(m_pipefd[0], F_SETFL, new_option);
        
        addsig(SIGPIPE, SIG_IGN,false);
        addsig(SIGALRM, sig_handler_wrapper ,false);
        alarm(5);
    }
    void sig_handler(int sig){
        //为保证函数的可重入性，保留原来的errno
        int save_errno = errno;
        int msg = sig;
        //std::cout << "send timer out tick" << std::endl;
        LOG_DEBUG("Send Timer out tick");
        send(m_pipefd[1], (char *)&msg, 1, 0);
        errno = save_errno;
    }

    void addsig(int sig, void(handler)(int), bool restart)
    {
        struct sigaction sa;
        memset(&sa, '\0', sizeof(sa));
        sa.sa_handler = handler;
        if (restart)
            sa.sa_flags |= SA_RESTART;
        sigfillset(&sa.sa_mask);
        assert(sigaction(sig, &sa, NULL) != -1);
    }

    int & getfd0(){ return m_pipefd[0]; }
    int & getfd1(){ return m_pipefd[1]; }
private:
    int m_pipefd[2];
};

inline void sig_handler_wrapper(int sig){
    Timer::getInstance()->sig_handler(sig);
}

} // end namespace rojcpp
