#include "stdio.h"
#include "sys/types.h"
#include "unistd.h"
#include <stdio.h>
#include "sys/wait.h"
#include "stdlib.h"

//实现一个 ls | wc -l的命令功能
//思路: 主进程使用ls命令将标准输出重定向到fd[1](写端),然后再在子进程当中使用fd[0]读取得到数据进行操作
void homework2(){
    int fd[2], i;
    // char buf[1024];
    if(pipe(fd) == -1){
        perror("pipe create error\n");
        exit(1);
    }
    for(i = 0;i < 2; i++){
        pid_t pid = fork();
        if(pid == 0) break;
        else if(pid == -1){
            perror("fork error\n");
            exit(1);
        }
    }
    if(i == 1){
        //子进程1 wc -l 读取内容 
        close(fd[1]);   // 关闭无用的写端
        int ret = 0;
        dup2(fd[0], STDIN_FILENO);
        // int offset = 0;
        // while((ret = read(fd[0], buf + offset, sizeof(buf) - offset)) > 0){
        //     offset += ret;
        // }   // 积累这种read读取的操作 循环结束之后buf可以得到所有的数据
        close(fd[0]); // 关闭读端
        execlp("wc", "wc", "-l", NULL);
        perror("execlp wc -l error\n");
        exit(1);
    }else if(i == 0){
        //子进程0 ls -l 执行写端
        close(fd[0]);   //关闭无用的读端
        dup2(fd[1], STDOUT_FILENO); // 将标准输出重定向到管道的写端
        close(fd[1]);   //关闭了管道的写端，但是只是这个文件描述符关闭了，因为dup2会将标准输出也定向到管道的写端，所以还是可以通过标准输出将内容也写到管道中
        execlp("ls", "ls", "-l", NULL);
        perror("execl ls -l error\n");
        exit(1);
    }else {
        //父进程
        close(fd[1]);
        close(fd[0]);    //没有关闭fd[0] fd[1]
        while (1) {
            pid_t wpid = waitpid(-1, NULL, 0);
            if(wpid == -1) break;
        }   // 没有子进程就会返回-1，结束循环 
    }
}

int main(int argc, char* argv[]){
    homework2();
    return 0;
}