#include <bits/types/sigset_t.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

// 信号处理函数
void sigint_handler(int sig) {
    printf("\n收到 SIGINT 信号（信号编号：%d）\n", sig);
    printf("程序即将退出...\n");
    exit(0);
}

void print_pend(sigset_t* set){
    for(uint i = 0;i < 32;i++){
        if(sigismember(set, i)){
            putchar('1');
        }else putchar('0');
    }
    printf("\n");
}

/*
sigprocmaks(int how, const sigset_t * set, sigset_t * oldset);
how: SIG_BLOCK; mask = mask& set
		SIG_UNBLOCK;//取消阻塞，mask = mask & ~set;
		SIG_SETMASK;	// 用自定义的set替换mask
set: 自定义的set 
oldset: 旧有的mask
我的理解： 其实只要自定义的set当中有一个位置是1，表示这个位置想要进行改变（不管是阻塞或者取消阻塞）。
查看未决信号集：
int sigpending(sigset_t * set);
 	set: 传出的未决信号集

sigset_t set;自定义信号集
sigemptyset(sigset_t * set); 全部置为0
sigfillset(sigset* set); 全部置为1
sigaddset(sigset_t* set, int signum);
sigdelset(sigset_t* set, int signum);
*/

int main(int argc, char* argv[]){
    sigset_t set, pend_set;
    
    // 注册信号处理函数（可选，用于观察信号接收）
    signal(SIGINT, sigint_handler);
    
    // 初始化信号集
    sigemptyset(&set);
    sigaddset(&set, SIGINT);  // 2号 Ctrl+C 中断信号
    sigaddset(&set, SIGQUIT); // 3号 Ctrl+反斜杠
    sigaddset(&set, SIGBUS);   // 7号 总线错误

    //注意 下面两个参数是不能被屏蔽的，这里即使写上也没有用
    sigaddset(&set, SIGKILL);
    sigaddset(&set, SIGSTOP);
    
    // 屏蔽 SIGINT
    printf("=== 阶段1：屏蔽 SIGINT ===\n");
    printf("正在屏蔽 SIGINT 信号（Ctrl+C 将被忽略）...\n");
    int res = sigprocmask(SIG_BLOCK, &set, NULL);
    if(res == -1){
        perror("sigprocmask error");
        exit(1);
    }
    
    // 等待10秒，在这期间按 Ctrl+C 应该无效
    printf("屏蔽期间：请在10秒内尝试按 Ctrl+C（应该无效）\n");
    int t = 10;
    while (t--) {
        printf("剩余 %d 秒...\n", t + 1);
        sleep(1);
        //每秒打印一下未决信号集
        sigpending(&pend_set);
        print_pend(&pend_set);
    }
    
    // 检查未决信号
    sigset_t pending;
    sigpending(&pending);
    if (sigismember(&pending, SIGINT)) {
        printf("\n注意：检测到未决的 SIGINT 信号！\n");
        printf("（在屏蔽期间按了 Ctrl+C，取消屏蔽后将立即处理）\n");
    }
    
    // 取消屏蔽 SIGINT
    printf("\n=== 阶段2：取消屏蔽 SIGINT ===\n");
    printf("正在取消屏蔽 SIGINT 信号...\n");
    printf("现在可以按 Ctrl+C 来终止程序了\n");
    
    // 重要：使用包含 SIGINT 的 set 来取消屏蔽
    res = sigprocmask(SIG_UNBLOCK, &set, NULL);
    if(res == -1){
        perror("sigprocmask unblock error");
        exit(1);
    }
    // 如果之前有未决信号，这里会立即触发信号处理函数
    printf("程序正在运行，等待 Ctrl+C...\n");
    
    // 等待信号
    while (1) {
        pause();
    }
    
    return 0;
}