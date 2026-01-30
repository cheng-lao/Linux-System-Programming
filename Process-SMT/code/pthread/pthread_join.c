#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

struct stu{
    int age;
    char name[256];
    double money;
};

void* tfn(void* arg){
    struct stu *s;
/*
s = (struct stu *)malloc(sizeof(s));    
为什么看起来没崩溃：
    内存分配器可能分配了比请求更多的内存（对齐/最小块）
    未定义行为：越界访问可能暂时不崩溃，但行为不可预测
    可能覆盖了其他数据，但尚未触发崩溃
*/
    s = (struct stu *)malloc(sizeof(struct stu));
    s->age = 19;
    s->money = 5000.;
    strcpy(s->name, "Li Xin");

    printf("pthread: pid %d, tid: %lu\n", getpid(), pthread_self());
    pthread_exit(s);
}

int main(int argc, char* argv[]){
    pthread_t tid;
    struct stu* s;

    int ret = pthread_create(&tid,NULL, tfn, NULL);
    if(ret != -1){
        fprintf(stderr, "pthreat_create error: %s", strerror(ret));
        exit(1);
    }
    pthread_join(tid, (void **)&s);
    // join本质上是回收线程的类似于 子进程当中的wait函数，但是对线程的结束情况检查能力比较弱
    // 如果线程是被pthread_cancle杀死的，就只会返回-1在retval
    printf("get the retval: age: %d, name: %s, money: %.2lf\n", s->age, s->name, s->money);
    free(s);

    pthread_exit(0);
    // return 0;
}