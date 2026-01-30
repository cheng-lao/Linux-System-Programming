// #include <bits/pthreadtypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void func(void){
    // pthread_exit(NULL); // 如果在执行函数当中使用的pthread_exit后会直接结束线程
    return ;
    // 如果是return 的话就直接，返回到函数调用的位置
}

void* tfn_exit(void* arg){    
    func();             
    printf("pthread: pid %d, tid %lu\n", getpid(), pthread_self());
    // return NULL;     // 这里的return NULL,也就相当于退出线程
    pthread_exit(NULL); // 等效于 pthread_exit(NULL) 退出一个线程
}

/*
void pthread_exit(void *retval);
    retval是线程执行函数的返回值，对应到线程执行函数的返回参数是void*
The value pointed to by retval should not be located on the calling thread's stack, 
since the contents of that stack are undefined after the thread terminates.

The  pthread_exit() function terminates the calling thread and 
returns a value via retval that (if the thread is joinable) is available to another thread in the same process that calls pthread_join(3).
*/

int main(int argc,char* argv[]){
    pthread_t tid;

    printf("main: pid %d, tid %lu\n", getpid(), pthread_self());
    int res = pthread_create(&tid, NULL, tfn_exit, NULL);
    if(res != 0){
        fprintf(stderr, "res error: %s", strerror(res));
        exit(1);    // exit就相当于退出进程
    }
/*
exit() ; 退出当前进程
return: 返回到调用者那里去
pthread_exit(): 讲调用该函数的线程退出。
在多线程环境当中尽量避免使用exit，因为这会使得进程直接退出
*/
    pthread_exit(NULL); // 即使是在主线程当中调用pthread_exit函数也还是只结束主线程，并不会清空整个进程
//To allow other threads to continue execution, the main thread should terminate by calling pthread_exit() rather than exit(3).
    // 这一点与在主线程直接调用return有很大的区别，直接调用return会清空整个进程。
    // return 0;
}