#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

int main(){

    pid_t pid = fork();
    //常用的信号man 7 signal 可以查看
    // value    name    Action      处理方式
    // 1        SIGHUP  关闭终端    终止终端当中所有进程，
    if(pid == 0){
        printf("child pid is %d, ppid is %d\n", getpid(), getppid());
        sleep(1);
        kill(getppid(), SIGKILL);   // kill函数给目的进程发送信号       
    }else{
        printf("parent pid is %d\n", getpid());
        
    }
    
    return 0;
}