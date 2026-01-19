#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

int main() {
    /*
    MAP_PRIVATE (私有映射)
    解释： 这是一种“写时复制”（Copy-on-Write）机制。
    原理： 对内存的修改会被放入一个私有的副本中，不会写回磁盘文件，也不会被其他进程看到。
    权限要求： 即使映射区是 PROT_READ | PROT_WRITE，只要是私有映射，只需要 open 文件时有读权限即可（因为不会真的改动原文件）。
    */
    // 创建文件
    int fd = open("mmap.txt", O_CREAT | O_TRUNC | O_RDWR, 0644);
    ftruncate(fd, 4096);
    write(fd, "Original", 8);  // 写入初始内容
    
    // 父进程映射（MAP_PRIVATE）
    char *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
                   MAP_PRIVATE, fd, 0);
    printf("父进程：虚拟地址 = %p, 内容 = %s\n", p, p);
    
    pid_t pid = fork();
    
    if (pid == 0) {
        // 子进程
        printf("子进程：虚拟地址 = %p, 内容 = %s\n", p, p);
        // 注意：虚拟地址相同！
        
        // 子进程写入（触发 COW）
        strcpy(p, "Child modified");
        printf("子进程写入后：%s\n", p);
        
        sleep(2);  // 等待父进程读取
    } else {
        // 父进程
        sleep(1);  // 等待子进程写入
        
        printf("父进程读取：%s\n", p);
        // 输出：Original（看不到子进程的修改）
        
        // 父进程写入（触发 COW）
        strcpy(p, "Parent modified");
        printf("父进程写入后：%s\n", p);
        
        wait(NULL);
        munmap(p, 4096);
        close(fd);
    }
    
    return 0;
}