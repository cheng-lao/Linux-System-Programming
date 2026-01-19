
// #include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

int var = 100;

int main(int argc, char* argv[]){
    int fd = open("/dev/zero", O_RDWR);
    if(fd == -1){
        perror("open error!");
        exit(1);
    }
    
    int* p = (int*)mmap(NULL, sizeof(int), 
        PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if(p == MAP_FAILED){
        perror("mmap error!");
        exit(1);
    }
    close(fd);
    
    pid_t pid = fork();
    if(pid == 0){
        //子进程
        *p = 7000;
        var = 1000;
        printf("child mmap read data: %d, var is %d\n", *p, var);
        sleep(2);
    }else if(pid > 0){
        //父进程
        sleep(1);
        printf("parent mmap read data: %d, var is %d\n", *p, var);
        // parent mmap read data: 7000, var is 100 fork会共享映射区和文件描述符，
        // 但是对于其他变量，都是遵循读时共享，写时复制的原则
        wait(NULL);

        int ret = munmap(p, sizeof(int));
        if(ret == -1){
            perror("munmap error!");
            exit(1);
        }
    }
    
    return 0;
}