#include <signal.h>
#include <stdio.h>
#include "errno.h"
#include <sys/time.h>
#include "string.h"
#include <unistd.h>
/*
    struct itimerval 
    {
        struct timeval it_value;     // 定时器首次触发的时间（Time to the next timer expiration）
        struct timeval it_interval;  // 定时器重复触发的间隔时间（为0表示只触发一次）
    };
    struct timeval  //详细记录时间的结构体，秒和微秒 1s = 1000000us
    {
        __time_t tv_sec;         // 秒
        __suseconds_t tv_usec;   // 微秒（范围：0-999999）
    };
*/

void handler(int a){
    printf("hello world!\n");
}

int main(int argc, char* argv[]){

    struct itimerval timer;
    memset(&timer, 0, sizeof(timer));
    timer.it_value.tv_sec = 3;
    timer.it_value.tv_usec = 0;
    timer.it_interval.tv_sec = 1;
    timer.it_interval.tv_usec = 0;

    signal(SIGVTALRM, handler);
    int res = setitimer(ITIMER_VIRTUAL, &timer, NULL);
    if(res == -1){
        printf("Error code : %d\n", errno);
        perror("setitimer error!");
        // exit(1);

        res = setitimer(ITIMER_REAL, &timer, NULL);
        if (res == 0) {
            printf("ITIMER_REAL 工作正常\n");
        }
    }else{
        printf("ITIMER_VIRTUAL 设置成功\n");
    }

    printf("定时器已设置，等待触发...\n");
    int num = 1;
    while (1) {
        num++;
    }
    return 0;
}