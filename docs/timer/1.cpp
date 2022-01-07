#include <unistd.h>
#include <cassert>
#include <signal.h>
#include <cstring>
#include <cstdio>

void sig_handler(int sig){
    printf("Hello world\n");
}
int main(){
    
    alarm(5);
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = sig_handler;
    //if (restart)
        //sa.sa_flags |= SA_RESTART;
    sa.sa_flags = 0;
    sigfillset(&sa.sa_mask);
    assert(sigaction(SIGALRM, &sa, NULL) != -1);
    int count = 0;
    for(int i=1;i<=100;++i){
        sleep(1);
        printf("sleep 1\n");

    }
    return 0;
}
