#include "stdio.h"
#include "sys/stat.h"
#include <stdlib.h>
#include <errno.h>

int main(int argc, char* argv[]){

    if(argc != 2){
        fprintf(stderr, "Usage: %s [filepath]\n", argv[0]);
        exit(1);
    }

    // 创建一个fifo 指定名字的管道
    int ret = mkfifo(argv[1], 0664);    //创建一个pipe文件并且设定权限为644
    if(ret == -1){
        perror("mkfifo error");
        if(errno == EPERM || errno == EOPNOTSUPP){
            fprintf(stderr, "提示: Windows文件系统(NTFS)不支持FIFO。\n");
            fprintf(stderr, "建议: 请在Linux原生文件系统中创建FIFO，例如: /tmp/myfifo\n");
        }
        exit(1);
    }

    printf("FIFO创建成功: %s\n", argv[1]);
    return 0;
}