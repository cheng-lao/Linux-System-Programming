/*
 * 读写锁学习示例
 * 演示：多个读者线程并发读共享数据，写者线程独占更新
 * 编译: gcc -o rwlock-demo rwlock-demo.c -lpthread
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define READER_COUNT  4
#define WRITER_COUNT  2
#define BUFFER_SIZE   64

static pthread_rwlock_t rwlock;
static char shared_buffer[BUFFER_SIZE];
static int  shared_version;

/* 读者线程：多次加读锁读取共享缓冲区 */
static void *reader_thread(void *arg)
{
    int id = *(int *)arg;
    char local_buf[BUFFER_SIZE];
    int i, ver;

    for (i = 0; i < 10; i++) {
        pthread_rwlock_rdlock(&rwlock);
        ver = shared_version;
        strncpy(local_buf, shared_buffer, BUFFER_SIZE - 1);
        local_buf[BUFFER_SIZE - 1] = '\0';
        pthread_rwlock_unlock(&rwlock);

        printf("[Reader %d] version=%d, content=\"%s\"\n", id, ver, local_buf);
        usleep(10000 + (rand() % 50000));
    }
    return NULL;
}

/* 写者线程：加写锁更新共享缓冲区 */
static void *writer_thread(void *arg)
{
    int id = *(int *)arg;
    int i;

    for (i = 0; i < 10; i++) {
        pthread_rwlock_wrlock(&rwlock);
        shared_version++;
        snprintf(shared_buffer, BUFFER_SIZE, "written by writer %d, round %d", id, i + 1);
        printf("[Writer %d] updated to version %d\n", id, shared_version);
        pthread_rwlock_unlock(&rwlock);
        usleep(50000 + (rand() % 100000));
    }
    return NULL;
}

int main(void)
{
    pthread_t readers[READER_COUNT], writers[WRITER_COUNT];
    int reader_ids[READER_COUNT], writer_ids[WRITER_COUNT];
    int i;

    srand((unsigned)time(NULL));
    strcpy(shared_buffer, "initial content");
    shared_version = 0;

    if (pthread_rwlock_init(&rwlock, NULL) != 0) {
        perror("pthread_rwlock_init");
        return 1;
    }

    for (i = 0; i < READER_COUNT; i++) {
        reader_ids[i] = i;
        if (pthread_create(&readers[i], NULL, reader_thread, &reader_ids[i]) != 0) {
            perror("pthread_create reader");
            pthread_rwlock_destroy(&rwlock);
            return 1;
        }
    }
    for (i = 0; i < WRITER_COUNT; i++) {
        writer_ids[i] = i;
        if (pthread_create(&writers[i], NULL, writer_thread, &writer_ids[i]) != 0) {
            perror("pthread_create writer");
            pthread_rwlock_destroy(&rwlock);
            return 1;
        }
    }

    for (i = 0; i < READER_COUNT; i++)
        pthread_join(readers[i], NULL);
    for (i = 0; i < WRITER_COUNT; i++)
        pthread_join(writers[i], NULL);

    pthread_rwlock_destroy(&rwlock);
    printf("Final version=%d, buffer=\"%s\"\n", shared_version, shared_buffer);
    return 0;
}
