#include "stdio.h"
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char* argv[]){
    char buf[1024];
    
    // if(argc != 2){
    //     perror("Usage: [filepath]");
    //     exit(1);
    // }
    printf("等待写进程打开 FIFO...\n");
    
    //使用fifo进行两个无血缘关系的进程间通信
    // 这里实现写端
    int fd = open("/tmp/myfifo", O_RDONLY);
    if(fd == -1){
        perror("open error");
        exit(1);
    }
    printf("FIFO 已打开，开始读入数据...\n");
    
    // write第三个参数是写入数据的大小，函数返回写入的数据量
    while (1) {
        int len = read(fd, buf, sizeof(buf));
        write(STDOUT_FILENO, buf, len);
    }
    close(fd);    
    return 0;
}