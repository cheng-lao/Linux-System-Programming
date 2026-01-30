#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
int main(int argc,char* agrv[]){
    pid_t p = fork();
    if(p > 0){
        exit(0);
    }else if(p < 0){
        perror("fork error!\n");
    }
    setsid();
    /* 这里如果不 fork */
    int fd =  open("/dev/tty", O_RDWR);  
    while (1);
    return 0;
}
