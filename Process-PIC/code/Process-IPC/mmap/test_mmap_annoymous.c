#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

int main() {
    // 创建文件
    /*  /dev/zero 是设备文件，不是普通文件,它提供“虚拟无限大”的零字节流 ,内核会按需分配物理内存，不需要预先设置文件大小
    /dev/zero是设备文件，不是普通文件,每个进程打开 /dev/zero 得到不同的文件描述符,设备文件的映射是独立的，不共享物理内存
    // int fd = open("/dev/zero", O_RDWR); // 如果使用/dev/zero这个文件不用加MAP_ANONYMOUS
    if(fd == -1){
        perror("open error!");
        exit(1);
    }
    // //ftruncate(fd, 4096);
    // write(fd, "Original", 8);  // 写入初始内容 */
    
    // 匿名映射不关联任何文件，直接在内存中分配空间。使用速度比使用/dev/zero快
    // 但是这种不关联文件的匿名映射是不能在无血缘关系的进程间通信的，并且数据不能持久化
    char *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
                   MAP_SHARED|MAP_ANONYMOUS, -1, 0);
    
    if(p == MAP_FAILED){
        perror("mmap error!");
        exit(1);
    }
    else {
        strcpy(p, "Hello MMAP_ANONYMOUS!!");
    }
    printf("父进程：虚拟地址 = %p, 内容 = %s\n", p, p);
    
    pid_t pid = fork();
    
    if (pid == 0) {
        // 子进程
        printf("子进程：虚拟地址 = %p, 内容 = %s\n", p, p);
        // 注意：虚拟地址相同！
        
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
    }
    
    return 0;
}