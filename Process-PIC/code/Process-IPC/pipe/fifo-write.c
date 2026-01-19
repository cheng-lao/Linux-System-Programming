#include "stdio.h"
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>



int main(int argc, char* argv[]){
    char buf[1024] = "hello fifo!";
    int i = 0;
    // if(argc != 2){
    //     perror("Usage: [filepath]");
    //     exit(1);
    // }
    printf("等待读取进程打开 FIFO...\n");
    
    //使用fifo进行两个无血缘关系的进程间通信
    // 这里实现写端
    int fd = open("/tmp/myfifo", O_WRONLY);
    if(fd == -1){
        perror("open error");
        exit(1);
    }
    printf("FIFO 已打开，开始写入数据...\n");
    
    // write第三个参数是写入数据的大小，函数返回写入的数据量
    while (1) {
        sprintf(buf, "hello fifo! %d\n", i++);
        write(fd, buf, strlen(buf));
        sleep(1);
    }
    close(fd);
    
    return 0;
}