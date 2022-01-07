
#include <unistd.h>
#include <cassert>
#include <signal.h>
#include <cstring>
#include <cstdio>

void sig_handler(int sig){
    printf("Hello world\n");
}
int main(){
    printf("%d",SIGALRM);
    return 0;
}
