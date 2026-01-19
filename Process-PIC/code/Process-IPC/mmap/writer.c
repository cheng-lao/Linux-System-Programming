#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#define FILESIZE 1024

int main(int agrc, char* argv[]){

    // int fd = open("mmap.txt", O_RDWR | O_CREAT | O_TRUNC, 0644);
    int fd = open("/dev/zero", O_RDWR);
    int i = 0;
    if(fd == -1){
        perror("open error");
        exit(1);
    }
    ftruncate(fd, FILESIZE);
    
    char* p = (char*) mmap(NULL, FILESIZE,
         PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    
    if(p == MAP_FAILED){
        perror("mmap error");
        close(fd);
        exit(1);
    }

    printf("写入程序：虚拟地址 = %p\n", p);
    
    while (1) {
        sprintf(p,"Message %d from writer; ", i++);
        printf("写入进程发送：%s\n", p);
        sleep(2);
        if(strncmp(p,  "quit", 4) == 0){
            break;
        }
        // scanf("%s",p);
    }
    munmap(p, FILESIZE);
    close(fd);
    
    return 0;
}