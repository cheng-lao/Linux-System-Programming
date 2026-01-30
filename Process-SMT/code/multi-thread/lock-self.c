// #include <bits/pthreadtypes.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
/*
死锁现象浮现：
死锁是一种使用锁不恰当的现象。
    第一种情况是： 一个线程对一个所重复加锁
    第二种情况是： 两个线程各自持有一把锁，请求另一把。（有一个循环请求状态）
*/
pthread_mutex_t mutex1; // 负责资源1 的互斥访问，
pthread_mutex_t mutex2; // 负责资源2 的互斥访问

int r1, r2;
void* tfn2(void * arg){
    while (1) {
        pthread_mutex_lock(&mutex1);  // 线程先获取 mutex1
        sleep(rand() % 3);
        printf("thread---r1: %d\n", r1++);
        pthread_mutex_lock(&mutex2);  // 然后尝试获取 mutex2
        printf("thread---r2: %d\n", r2++);
        pthread_mutex_unlock(&mutex2);
        pthread_mutex_unlock(&mutex1);
    }   
    pthread_exit(NULL);
}

pthread_mutex_t mutex;
void* tfn1(void *agr){
    sleep(1);
    printf("before self lock, pid: %d, tid: %lu\n", getpid(), pthread_self());
    pthread_mutex_lock(&mutex);
    printf("after self lock, pid: %d, tid: %lu\n", getpid(), pthread_self());    
    pthread_exit(NULL);
}

int main(int argc, char* argv[]){
    pthread_t tid1, tid2;
    srand(time(NULL));  // 初始化随机数种子
    pthread_mutex_init(&mutex1, NULL);
    pthread_mutex_init(&mutex2, NULL);
    pthread_mutex_init(&mutex, NULL);

    int ret = pthread_create(&tid1, NULL, tfn2, NULL);
    ret = pthread_create(&tid2, NULL, tfn1, NULL);
    if(ret != 0){
        fprintf(stderr, "pthread_create error:%s", strerror(ret));
        exit(1);
    }
    while (1) {
        pthread_mutex_lock(&mutex2);  // 主线程先获取 mutex2
        sleep(rand()%3);
        printf("main---r1: %d\n", r1++);
        pthread_mutex_lock(&mutex1);  // 然后尝试获取 mutex1
        printf("main---r2: %d\n", r2++);
        pthread_mutex_unlock(&mutex1);
        pthread_mutex_unlock(&mutex2);
    }

    pthread_join(tid1, NULL);   //回收子线程
    pthread_join(tid2,NULL);

    pthread_mutex_destroy(&mutex1);        //销毁锁
    pthread_mutex_destroy(&mutex2);
    pthread_mutex_destroy(&mutex);

    return 0;   
}