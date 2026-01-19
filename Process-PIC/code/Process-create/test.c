#include "stdio.h"
// execl函数的被调用函数
int main(int argc, char* argv[]){

    printf("Hello World! Execl function work well!\n");
    printf("Num of agrc is :%d\n", argc);
    for(int i = 0; i < argc; i++){
        printf("agrv[%d] is :%s\n", i, argv[i]);
    }
    return 0;
}