#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

pthread_cond_t has_product = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

struct message{
    struct message* next;
    int data;    
};
struct message* mp = NULL;  // 链表头（哑元头节点，mp->next 为第一个产品）

void* produce(void * arg){
    while (1) {
        struct message* p = (struct message*)malloc(sizeof(struct message));
        //开始生产东西
        p->data = rand() % 100;
        pthread_mutex_lock(&mutex);
        p->next = mp->next;
        mp->next = p;   //头插法
        printf("Put data ------------------%d\n", p->data);
        pthread_mutex_unlock(&mutex);
        pthread_cond_signal(&has_product);
        sleep(rand() % 3);      // 模拟生产者耗时
    }
    pthread_exit(NULL);
}

void* consume(void* agrc){
    while (1) {
        pthread_mutex_lock(&mutex);
        // 这里必须要使用while!! 重新获得锁之后，wait函数返回，但是此时还有可能条件没有满足
        while (mp->next == NULL) {  // 没有产品时等待（空链表）
            pthread_cond_wait(&has_product, &mutex); // 函数返回的时候一定是有锁的
            // wait函数干三件事
            // 1. 阻塞 2.释放锁 3. 等待唤醒信号，然后重新结束阻塞态，重新获得锁之后再返回 
        }
        // 有产品，此时持有锁
        struct message* p = mp->next;
        mp->next = p->next;
        printf("Get product---- data: %d\n", p->data);
        pthread_mutex_unlock(&mutex);
        free(p);
    }
}

int main(int agrc, char* agrv[]){

    pthread_t producer, consumer;

    // 初始化哑元头节点，避免生产者里对 mp 解引用 NULL
    mp = (struct message*)malloc(sizeof(struct message));
    mp->next = NULL;
    mp->data = 0;

    srand((unsigned)time(NULL));

    int ret = pthread_create(&producer, NULL, produce, NULL);
    if(ret != 0){
        fprintf(stderr, "pthread create producer %s\n", strerror(ret));
        exit(1);
    }
    ret = pthread_create(&consumer, NULL, consume, NULL);
    if(ret != 0){
        fprintf(stderr, "pthread create consumer %s\n", strerror(ret));
        exit(1);
    }

    ret = pthread_join(consumer, NULL);
    if(ret != 0){
        fprintf(stderr, "pthread_join consumer %s\n", strerror(ret));
        exit(1);
    }
    ret = pthread_join(producer, NULL);
    if(ret != 0){
        fprintf(stderr, "pthread_join producer %s\n", strerror(ret));
        exit(1);
    }

    return 0;
}