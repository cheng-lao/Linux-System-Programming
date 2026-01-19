#include "sys/types.h"
#include "unistd.h"
#include <stdio.h>
// #include <string.h>
#include "sys/wait.h"
#include "stdlib.h"
#include "fcntl.h"



//Loop create 5 Child Process and identify every process
void test_fork(void){
    pid_t pid;
    int i;
    for(i = 0;i < 5;i++){
        pid = fork();
        if(pid == 0){ //子进程
            sleep(i);   //sleep设置控制执行顺序
            printf("I'm %dth child, pid is%d,parent_pid is %d \n",
                 i, getpid(), getppid());
            break;
        }else if(pid < 0){
            perror("fork error");
        }
    }
    if(i == 5) {
        sleep(5);
        printf("I'm parent!\n");
    }
}

//测试执行execl簇函数
void test_execl(){
    pid_t pid = fork();
    if(pid == 0){
        printf("execl函数执行第三方可执行文件\n");
        //execl函数的第一个执行文件的路径， 第二个可执行文件的参数
        //一般在使用命令的时候第一个命令的名称就是第一个参数后面以此类推，最后是NULL结尾
        // execl("./main", "./main", NULL);  //最好不要调用自己，叠加fork会有资源消耗
        execl("./test", "arg1", "arg2", "arg3", NULL);//路径也就是当前文件的路径
        //如果成功执行成功的话，不会返回执行接下来的代码，所以这里可以直接打印错误.
        perror("子进程execl函数执行失败\n");
        exit(1);    //如果子进程执行execl函数失败，那么就需要返回
    }else if(pid > 0){
        printf("父进程等待执行\n");
        wait(NULL); //回收子进程的PID号
        printf("子进程结束执行\n");
    }else{
        perror("fork失败");
    }
    // 父进程最后执行execlp函数
    execlp("ls","ls","-l","/mnt", NULL);
}

//实现一个命令重定向
void test_dup_execl(const char* file_path){
    //创建一个新的文件
    // O_WRONLY: 只写
    // O_CREAT: 如果文件不存在就创建
    // O_TRUNC: 如果文件存在，就清空它 (这就是 > 和 >> 的区别)
    // 0644: 文件权限 (rw-r--r--)
    int fd = open(file_path, O_CLOEXEC | O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (fd < 0) {
        perror("打开文件失败");
        exit(1);
    }
    //dup2函数：把标准输出替换成这个文件fd, 以后在标准输出(屏幕)当中输出的内容都会到fd
    dup2(fd, STDOUT_FILENO);
    close(fd);
    execl("/bin/ps","ps", "aux", NULL);
}

enum State {
    Orphan, // 孤儿进程 
    Zombie  // 僵尸进程
};
//测试出现僵尸进程和孤儿进程
void test_Orphan_Zombie(enum State s){
    pid_t pid = fork();
    if(pid == 0){
        //子进程 
        printf("子进程开始执行 pid:%d, ppid:%d\n", getpid(), getppid());   
        if(s == Orphan){
            sleep(3);
            printf("我是子进程 (PID: %d)，现在的父进程是 %d (我被收养了)\n", getpid(), getppid());
            while(1) {
                //循环过程当中ps -eo pid ppid stat cmd | grep main(可执行文件的名称)
                //11147     1 S    ./main
                sleep(1);
            }
        }
        printf("子进程结束执行\n");
        exit(0);
    }else{
        printf("父进程开始执行 pid:%d \n", getpid());
        if(s == Zombie)
            sleep(30);
        //ps -eo pid,ppid,stat,cmd | grep Z
        //11649 11648 Z    [main] <defunct> (第三个参数是状态Z表示Zombie)
        printf("父进程结束执行\n");
    }
}

//wait函数
void test_wait(){
    pid_t pid, wpid;
    int status;
    pid = fork();
    if(pid == 0){
        //子进程
        printf("子进程开始执行，pid: %d\n", getpid());
        sleep(2);
        printf("-------------child die--------------\n");
        exit(77);   // status在[0, 255]之间合法
    }else if(pid > 0){
        //父进程
        printf("父进程开始执行\n");
        wpid = wait(&status);
        //通过status
        if(wpid == -1){
            perror("wait error");
            exit(1);
        }
        // 是否正常推出后
        if(WIFEXITED(status)){
            int exit_code = WEXITSTATUS(status);
            printf("exit_code is :%d\n", exit_code);
        }
        // 是否被信号终止
        if(WIFSIGNALED(status)){
            int sig_num = WTERMSIG(status); 
            printf("子进程被信号终止，信号编号：%d\n",sig_num);
            switch (sig_num) {
                case SIGTERM:   // 
                    printf("子进程收到 SIGTERM 信号\n");
                    break;
                case SIGKILL:  // 9 - 强制终止
                    printf("子进程被强制终止 (SIGKILL)\n");
                    break;
                case SIGSEGV:  // 11 - 段错误
                    printf("子进程发生段错误 (SIGSEGV)\n");
                    break;
                default:
                    printf("子进程被信号 %d 终止\n", sig_num);
            }
            if(WCOREDUMP(status)){
                printf("子进程产生了核心转储文件\n");
            }
        }
    }
}

void test_waitpid1(){
    pid_t pid1, pid2;
    int status;
    
    // 创建第一个子进程
    pid1 = fork();
    if (pid1 == 0) {
        printf("子进程1开始执行，PID: %d\n", getpid());
        sleep(3);
        printf("子进程1执行完毕\n");
        exit(1);
    }
    // 创建第二个子进程
    pid2 = fork();
    if (pid2 == 0) {
        printf("子进程2开始执行，PID: %d\n", getpid());
        sleep(1);
        printf("子进程2执行完毕\n");
        exit(2);
    }
    // 父进程：先等待第二个子进程（先结束的）
    printf("父进程等待子进程2 (PID: %d)\n", pid2);
    waitpid(pid2, &status, 0);
    if (WIFEXITED(status)) {
        printf("子进程2已结束，退出码: %d\n", WEXITSTATUS(status));
    }
    // 再等待第一个子进程
    printf("父进程等待子进程1 (PID: %d)\n", pid1);
    waitpid(pid1, &status, 0);
    if (WIFEXITED(status)) {
        printf("子进程1已结束，退出码: %d\n", WEXITSTATUS(status));
    }
    printf("所有子进程都已结束\n");
}

//创建5个进程，实现轮询回收子进程
void test_waitpid2(){
    pid_t pid,wpid;
    int i = 0;
    int status;

    for (i = 0; i < 5; i++) {
        if(fork() == 0)
            break;  //主进程创建5个子进程        
    }
    if(i == 5){
        //main process
        printf("主进程执行开始,轮询回收子进程 pid: %d\n", getpid());
        while (1) {
            // 回收所有子进程
            wpid = waitpid(-1, &status, WNOHANG);
            // 返回值有pid(正常回收), 0(WNOHANG情况下的), -1(errno)
            if(wpid == 0){
                // 还有可回收的子进程没运行完
                printf("再等待1s 实现子进程结束\n");
                sleep(1);
            }else if(wpid > 0){
                printf("回收进程 pid: %d\n", wpid);
                if(WIFEXITED(status)){  // 正常结束
                    printf("child process end well! ,exit_code is %d\n", WEXITSTATUS(status));
                }else if(WIFSIGNALED(status)){  //信号异常终止
                    printf("signal kill the child process, signal is %d\n", WTERMSIG(status));
                }
            }else {
                perror("waitpid error\n");
                return;
            }
        }
    }else {
        //child process
        sleep(i);
        printf("child exit with code %d\n", i + 1);
        exit(i + 1);
    }
}

// 父进程fork3个子进程，三个子进程一个调用ps命令，一个调用自定义程序1(正常)，一个调用自定义程序2(会出段错误)。父进程使用waitpid对其子进程进行回收。
void homework1(){
    pid_t pid, wpid;
    int i = 0, status;
    for (i = 0;i < 3; i++) {
        pid = fork();
        if(pid == 0) break;
    }
    switch (i) {
        case 0:
            execlp("ps", "ps", "-al", NULL);
            perror("execlp ps error");
            exit(0);            
        case 1:
            execl("./test", "arg1", "arg2", "arg3", NULL);
            perror("execl ./test error");
            exit(1);
        case 2:
            execl("./error_test", NULL);
            perror("execl ./error_test error");
            exit(2);
        case 3:
            //父进程回收
            while (1) {
                wpid = waitpid(-1, &status, WNOHANG);
                if(wpid > 0){
                    if(WIFEXITED(status)){  // 正常返回的子进程
                        printf("exit code is %d\n", WEXITSTATUS(status));
                    }else if(WIFSIGNALED(status)){  // 特殊返回的子进程  段错误返回11
                        printf("exit with signal %d\n", WTERMSIG(status));
                    }
                }else if(wpid == 0){
                    sleep(1);
                    continue;
                }else{
                    printf("子进程全部回收完毕!\n");
                    break;
                }
            }
            break;
    }
}

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
    // test_fork();
    // test_execl();
    // test_dup_execl("temp.log"); //通过 dup完全可以实现ls -l > tmp.log这样的功能
    // test_Orphan_Zombie(Zombie);
    // test_waitpid1();
    // test_waitpid2();
    // homework1();
    homework2();
    return 0;
}