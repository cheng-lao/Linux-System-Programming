#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

// 这是一个清理函数，类似“遗言”
void cleanup_handler(void *arg) {
    printf("Worker: [Cleanup] 正在释放资源: %s\n", (char *)arg);
    // 在这里执行 unlock, close(fd), free(ptr) 等操作
}

void *worker_thread(void *arg) {
    // 1. 注册清理函数
    // 如果线程在 pop 之前被 cancel，这个函数会被自动调用
    pthread_cleanup_push(cleanup_handler, "我的重要资源");
    printf("Worker: 线程开始运行，进入循环...\n");
    int i = 0;
    while (1) {
        printf("Worker: 正在工作 %d\n", i++);
        
        // sleep 是一个标准的“取消点”
        // 线程只会在这一行暂停时检查有没有收到 cancel 信号
        sleep(1); 
    }

    // 2. 移除清理函数
    // 参数 0 表示：现在弹出这个函数，但不要执行它（因为我要正常结束了）
    // 如果填 1，表示弹出并立即执行
    pthread_cleanup_pop(0); 

    return NULL;
}

int main() {
    pthread_t tid;
    void *res;

    // 创建线程
    if (pthread_create(&tid, NULL, worker_thread, NULL) != 0) {
        perror("Create thread failed");
        exit(1);
    }

    // 让子线程跑 3 秒
    sleep(3);

    printf("Main: 发送 Cancel 信号...\n");
    // 发送取消请求
    pthread_cancel(tid);

    // 等待线程结束（回收尸体）
    pthread_join(tid, &res);

    // 检查线程是怎么结束的
    if (res == PTHREAD_CANCELED) {
        printf("Main: 子线程已被成功 Cancelled。\n");
    } else {
        printf("Main: 子线程正常退出（这在当前代码不应该发生）。\n");
    }

    return 0;
}