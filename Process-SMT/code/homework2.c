#include "stdio.h"
// #include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
// #include"sys/mman.h"
#include <sys/wait.h>


//计算一下一秒钟电脑能数多少个数
int num = 0;  // 全局变量，用于在信号处理函数中访问

// SIGALRM 信号处理函数
void handler(int sig){
    printf("一秒钟数了 %d 个数\n", num);
    exit(0);
}

int main(int argc, char* agrv[]){

    // // 注册信号处理函数，当收到 SIGALRM 信号时执行 handler
    // signal(SIGALRM, handler);
    
    // // 设置1秒的闹钟，1秒后向当前进程发送 SIGALRM 信号
    // alarm(1);
    
    // // 不断计数，直到收到 SIGALRM 信号
    // while(1) {
    //     num++;
    // }
    alarm(1);
    int num = 0;
    while(1){
        printf("%d\n", ++num);
    }

    return 0;
}