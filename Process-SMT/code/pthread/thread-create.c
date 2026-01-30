#include <stddef.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void* thread_func(void* arg){
    //打印之后会发现，进程ID是相同的，但是线程ID是不同的。
    //这是因为线程是共享进程资源的，同属于一个进程，但是有自己的独立的栈帧和线程ID
    printf("thread_func: pid %d, thread id: %lu\n", getpid(), pthread_self());    
    return NULL;
}

void* thread_func_arg(void* arg){
    int i = (int) arg;
    //这里不要传递地址，因为主线程的运行时速度过快会更改局部变量i的内容，
    // 这样即使拿到了i的地址，也是取不到真实的值的
    printf("thread func: i am %dth, pid %d, thread id: %lu\n", i+1, getpid(), pthread_self());
    sleep(i);
    return NULL;
}

int main(){

    //区别一下LWP 线程号和tid线程id的区别，
    //LWP是面向CPU的线程号，在CPU调度当中是使用LWP线程号进行调度的
    //线程ID是面向用户空间编程使用的
    //介绍两个函数 pthraed_create() 和 pthread_slef()
    pthread_t tid;
    int i = 0;

    printf("main: pid: %d, thread id: %lu \n", getpid(), pthread_self());
    //On success, pthread_create() returns 0; on error, it returns an error  number,  and  the  contents  of
    // *thread are undefined.
    int res = pthread_create(&tid, NULL, thread_func, NULL);
    if(res != 0){
        fprintf(stderr, "pthread_error: %s\n", strerror(res));
        exit(1);
    }
    sleep(1);

    for(i = 0;i < 5; i++){
        int res = pthread_create(&tid, NULL, thread_func_arg, i);
        if(res != 0){
            fprintf(stderr, "pthread_create error.\n");
            break;
        }
    }

    sleep(i);
    return 0;
}