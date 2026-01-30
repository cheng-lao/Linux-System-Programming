#include "stdio.h"
#include <signal.h>
// #include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
// #include"sys/mman.h"
#include <sys/wait.h>
#define CHILD_NUM 5

//循环创建5个子进程，父进程使用kill函数终止任意一个子进程。
int main(int argc,char* argv[]){
    pid_t children[CHILD_NUM];
    int i = 0;

    //父进程需要先创建一个通信的文件内容
    // pid_t* p_child = (pid_t*) mmap(NULL, sizeof(children), PROT_READ | PROT_WRITE,
    //  MAP_SHARED | MAP_ANON, -1, 0);
    // if(p_child == MAP_FAILED){
    //     perror("mmap error!");
    //     exit(1);
    // }

    for(i = 0;i < CHILD_NUM; i++){
        pid_t p = fork();
        if(p == 0)break;
        else children[i] = p;
    }
    if(i == CHILD_NUM){
        //main process
        //select num 2 child process to kill
        kill(children[2], SIGKILL);
        while(1){
            int status;
            pid_t t = wait(&status);
            if(t == -1) break;  // 回收所有进程
            if(WIFSIGNALED(status)){
                printf("Child %d exited with signal %d\n", t, WTERMSIG(status));
            }else{
                printf("Child %d exited normally\n", t);
            }
        }
    }else{
        int n = 4;
        while (n--) {
            printf("i am child %d, my pid is %d and parent pid is %d\n", i, getpid(), getppid());
            sleep(1);
        }        
    }

    return 0;
}