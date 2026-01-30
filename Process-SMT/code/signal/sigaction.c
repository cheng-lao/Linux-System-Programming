
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void handler(int signum){
    printf("catch signal num is %d\n", signum);    
    return ;
}

int main(int argc, char* argv[]){
    struct sigaction act, oldact;
    act.sa_handler = handler;
    // 设置信号阻塞集，生命周期是执行调用函数的时候，当函数返回阻塞集回归原来的状态
    sigemptyset(&act.sa_mask);
    //遵循默认设置，当执行调用函数的时候，会默认阻塞触发调用函数的信号  
    act.sa_flags = 0;       

    int res = sigaction(SIGINT, &act, &oldact);
    if(res == -1){
        perror("sigaction error");
        exit(1);
    }
    while (1) pause();
    return 0;
}