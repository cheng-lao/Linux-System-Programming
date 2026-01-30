
#include <bits/types/sigset_t.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#define CHILD_NUM 10
void catch_child(int signum){
    int wpid;
/*
    while((wpid = wait(NULL))!=-1){
        printf("----------Catch child %d!\n", wpid); 
    } // 这个写法并不好，因为当有子进程还没执行完的时候wait会一直在阻塞，所以一旦跳出这个循环，说明子进程全部被回收了
     但是父进程相当于一直在阻塞等待回收子进程，这不合适
 */
    // waitpid 只有当有进程可以回收的时候才会返回大于0的数字，其他时候都是小于等于0
    while ((wpid = waitpid(-1, NULL, WNOHANG)) > 0) {
        printf("Catch child! Wpid is %d\n",wpid);
    }
    return ;
}

//一个是信号注册函数在哪里定义的问题 建议在fork之前设置注册函数
//另一个就是捕捉函数内部的实现逻辑 建议使用waitpid + WNOHANG组合
int main(){
    
    pid_t pid;
    int i = 0;

/*
信号的处理函数定义在这里是为了防止子进程全死完了还没注册，这样的话所有的SIGCHLD信号的执行动作都是忽略
所以这里在注册之前提前注册一下处理函数。
（这是一个解决方法, 还有另一个就是在创建子进程之前将屏蔽信号SIGCHLD，这样的话信号就会被放在未决信号集等待处理）
    struct sigaction act;
    act.sa_handler = catch_child;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGCHLD, &act, NULL);
*/
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);
    sigprocmask(SIG_BLOCK, &set, NULL);

    for(i = 0;i < CHILD_NUM; i++)
        if((pid = fork()) == 0)
            break;
    
    if(i == CHILD_NUM){
        //父进程
        printf("parent pid is %d\n", getpid());
        sleep(1);
        struct sigaction act;
        act.sa_handler = catch_child;
        sigemptyset(&act.sa_mask);
        act.sa_flags = 0;
        sigaction(SIGCHLD, &act, NULL);
        
        sigprocmask(SIG_UNBLOCK, &set, NULL);

        while (1) {
            //注意点1，父进程不能在子进程结束之前死亡，否则父进程的代码区销毁，就不能执行捕捉函数了
            pause();
        }
    }else{
        //子进程
        printf("child pid is %d\n", getpid());
    }
    

    return 0;
}