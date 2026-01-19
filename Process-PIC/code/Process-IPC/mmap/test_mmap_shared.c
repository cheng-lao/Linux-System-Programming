#include "stdio.h"
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

void sys_err(const char* str){
    perror(str);
    exit(1);
}

int main(int argc, char* argv[]){
    char *p = NULL;
    int fd;
/*
mmap() 只能映射文件中已存在的部分。
如果文件为空或小于映射长度，映射会失败或无法访问超出文件大小的区域。
void *mmap(void *addr, size_t length, int prot, int flags,
                  int fd, off_t offset);
*/
    fd = open("mmap.txt", O_CREAT|O_TRUNC|O_RDWR, 0644);
    if(fd == -1){
        sys_err("open error");
    }
    
    int len = 4096;
    ftruncate(fd, len);
    p = mmap(NULL, len,
         PROT_READ| PROT_WRITE, MAP_SHARED, fd, 0);
    if(p == MAP_FAILED){
        sys_err("mmap error");
    }
    strcpy(p + len, "hello mmap");
    printf("写入的内容是:  %s", p);
    
    munmap(p, 4096);
    close(fd);
    return 0;
}