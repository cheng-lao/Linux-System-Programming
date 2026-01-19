

#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define FILESIZE 1024
int main(int argc, char* argv[]){
    
    // int fd = open("mmap.txt", O_RDWR);
    int fd = open("/dev/zero", O_RDWR);
    if(fd == -1){
        perror("open error");
        exit(1);
    }
    char* p = mmap(NULL, FILESIZE,
         PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(p == MAP_FAILED){
        perror("mmap error");
        close(fd);
        exit(1);
    }
    printf("读取进程：虚拟地址 = %p\n", p);
    // 注意：虚拟地址可能不同，但物理页是共享的！
    
    char last_msg[FILESIZE] = "";
    while (1) {
        printf("Current p'content is %s", p);
        if(strcmp(p, last_msg) != 0){
            printf("收到数据 %s\n", p);
            strcpy(last_msg, p);
        }
        sleep(1);
        if(strstr(p, "Message 10") != NULL){
            strcpy(p, "quit");
            break;
        }
    }
    munmap(p, FILESIZE);
    close(fd);

    return 0;
}