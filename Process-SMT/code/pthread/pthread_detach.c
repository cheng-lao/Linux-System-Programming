// #include <bits/pthreadtypes.h>
#include <stddef.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define USE_ATTR

void* thread_func(void* arg){
    //打印之后会发现，进程ID是相同的，但是线程ID是不同的。
    //这是因为线程是共享进程资源的，同属于一个进程，但是有自己的独立的栈帧和线程ID
    printf("thread_func: pid %d, thread id: %lu\n", getpid(), pthread_self());    
    
    // 如果在子线程当中执行detach，意思是不用让主线程来回收子线程，子线程结束让系统自动回收
    // 不能重复设置detach子线程
    int res = pthread_detach(pthread_self());
    if(res != 0){
        fprintf(stderr, "pthread_detach error: %s", strerror(res));
        exit(1);
    }
    return NULL;
}

int main(){
    pthread_t tid;
/* 
    也可以这么创建，在进程一开始的时候就设置detach 分离属性
    过程类似于
    对 sigset的使用方式
        sigset_t sig;
        sigemptyset(&sig);
        sigaddset(&sig, SIGKILL);
        sigprocmask(SIG_BLOCK, &sig, NULL);
*/
#ifdef USE_ATTR
    pthread_attr_t attr;
    int res = pthread_attr_init(&attr);
    if(res != 0){
        fprintf(stderr, "pthread_attr_init error: %s\n", strerror(res));
        exit(1);
    }
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    res = pthread_create(&tid, &attr, thread_func, NULL);
    if(res != 0){
        fprintf(stderr, "pthread_create error: %s\n", strerror(res));
        exit(1);
    }
    // ......执行了一系列操作
    res = pthread_attr_destroy(&attr); //在操作完之后进行销毁
    if(res != 0){
        fprintf(stderr, "pthread_destroy error: %s\n", strerror(res));
        exit(1);
    }
    sleep(1);
#else

    int res = pthread_create(&tid, NULL, thread_func, NULL);
    if(res != 0){
        fprintf(stderr, "pthread_error: %s\n", strerror(res));
        exit(1);
    }
    
    res = pthread_detach(tid);
    if(res != 0){
        fprintf(stderr, "pthread_detach error: %s", strerror(res));
        exit(1);
    }

#endif
    res = pthread_join(tid,NULL);
    //默认阻塞回收子进程，回收的
    if(res != 0){
        fprintf(stderr, "pthread_join error: %s\n", strerror(res));
        fprintf(stderr, "After detach, thread can't be joined!\n");
        exit(1);    
    }
    pthread_exit(0);
    // return 0;
}