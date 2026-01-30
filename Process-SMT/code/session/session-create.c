#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>

/*
1. fork子进程，让父进程终止
2. 子进程调用setsid（）创建新会话
3. 第二次 Fork (彻底禁止获取终端)
4. 通常根据需要，改变工作目录位置chdir()
5. 通常根据需要，重设umask文件权限掩码
6. 通常根据需要，关闭/重定向(dup) 文件描述符
7. 守护进程， 业务逻辑 
*/

int main(int argc, char* argv[]){

    //创建一个守护进程
    pid_t pid;
    int fd;

    pid = fork();
    if(pid > 0)
        return 0;

    pid = setsid();
    if(pid == -1){
        perror("setsid error");
        exit(1);
    }
    
    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS); // 第一代子进程退出
    }
    
    chdir("/home");
    umask(0022);    //修改umask掩码，守护进程会继承父进程的mask，为了拜托父进程的影响，所以重新设置
    //每当创建一个 文件或者是文件夹的时候，系统会为其赋予权限 也就是 777 - umask，
    
    fd = open("/dev/null", O_RDWR);
    // close(STDIN_FILENO);
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);

    //后续开始执行守护进程的一系列逻辑
    while (1) {
        // 模拟后台工作
        // 注意：千万不要用 printf/std::cout，因为 stdout 已经被丢弃了
        // 应该使用 syslog 或者写入自己的日志文件
        sleep(5); 
    }
/*
偷懒的方法：daemon() 函数
Linux 提供了一个非标准（但在 Linux/BSD 上通用）的库函数，帮你把上面的活儿全干了：
#include <unistd.h>
int daemon(int nochdir, int noclose);
nochdir: 如果填 0，会自动帮你 chdir("/")。
noclose: 如果填 0，会自动帮你把 0, 1, 2 重定向到 /dev/null。
*/

    return 0;
}