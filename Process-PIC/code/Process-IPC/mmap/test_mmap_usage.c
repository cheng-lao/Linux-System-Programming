#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


int main(int argc, char* argv[]){

    int fd = open("/dev/zero", O_RDWR);
    if(fd == -1){
        perror("open error!");
        exit(1);
    }
    ftruncate(fd, BUFSIZ);
    
    char* p = mmap(NULL, BUFSIZ, 
        PROT_WRITE|PROT_READ, MAP_SHARED, fd, 2);
    //mmap error!: Permission denied 不管使用什么参数（MAP_SHARED还是MAP_PRIVATE）open文件必须要有读权限
    //mmap error!: Invalid argument offset必须是使用4096的整数倍，否则硬件无法进行地址转换，errno 为 EINVAL
    //如果使用 MAP_SHARED参数，mmap的权限必须是open文件权限的子集
    if(p == MAP_FAILED){
        perror("mmap error!");
        exit(1);
    }
    

    close(fd); //映射成功后可立即关闭 fd,后续操作可以只通过p来操作

    //munmap 释放地址必须精确。第一个参数必须是 mmap 成功返回的原始起始地址。
    munmap(p, BUFSIZ); 
    
    return 0;
}