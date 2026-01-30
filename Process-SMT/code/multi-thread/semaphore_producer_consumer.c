// #include <bits/pthreadtypes.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define CONSUMER_NUM 5

sem_t product_num;
sem_t blank_num;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
void sys_err_pthread(const char* s,const int ret){
    fprintf(stderr, " pthread %s: %s", s, strerror(ret));
    return ;
}

void sys_err(const char* s){
    perror(s);
    exit(1);
}

struct message{
    struct message* next;
    char data[100];  
};

struct message* msg = NULL; //共享数据区

void* consumer_func(void* arg){
    const int i = (int)arg;
    struct message* m = NULL;
    while (1) {
        sem_wait(&product_num);     //P(product)
        
        pthread_mutex_lock(&mutex); // 保证互斥访问 链表
        m = msg->next;
        msg->next = m->next;
        printf("Consumer%d consume %s \n",i , m->data);
        pthread_mutex_unlock(&mutex);

        free(m);
        sem_post(&blank_num);       //V(blank)
    }
    pthread_exit(NULL);
}

void* producer_func(void * arg){
    int i = 0;
    while (++i) {
        //准备数据
        struct message* m = (struct message*) malloc(sizeof(struct message));
        sprintf(m->data, "num: %d", i);
        printf("Producer produce: %d\n", i);
        
        //确保有位置
        sem_wait(&blank_num);
        
        //保证对链表的互斥访问
        pthread_mutex_lock(&mutex);
        m->next = msg->next;
        msg->next = m;
        pthread_mutex_unlock(&mutex);
        //释放signal 提示有产品出现
        sem_post(&product_num);
        usleep(500000);
    }
    pthread_exit(NULL);
}

int main(int argc,char* argv[]){

    int ret;
    pthread_t producer, consumer[CONSUMER_NUM];
    ret = sem_init(&product_num, 0, 0);
    if(ret != 0)
        sys_err_pthread("sem init error", ret);
    ret = sem_init(&blank_num, 0, 8);
    if(ret != 0){
        sys_err_pthread("sem init error", ret);
    }

    // 初始化头结点
    msg = (struct message *)malloc(sizeof(struct message));
    memset(msg->data, 0, sizeof(msg->data));
    msg->next = NULL;

    ret = pthread_create(&producer, NULL, producer_func, NULL);
    if (ret != 0) 
        sys_err_pthread("pthread_create producer error", ret);
    
    for (int i=0; i<CONSUMER_NUM; i++) {
        ret = pthread_create(&consumer[i], NULL,
             consumer_func, (void**)i);
        if (ret != 0){
            sys_err_pthread("pthread_create consumer error", ret);
        } 
    }

    pthread_join(producer, NULL);
    for (int i=0; i<CONSUMER_NUM; i++) {
        pthread_join(consumer[i],NULL);
    }

    pthread_mutex_destroy(&mutex);
    sem_destroy(&product_num);
    sem_destroy(&blank_num);

    return 0;
}