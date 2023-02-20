#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc,char* argv[]){
    // char *parseargv[3];
    // parseargv[0] = "sleep";
    // parseargv[1] = argv[1];
    // parseargv[2] = 0;
    if(argc!=2){
        printf("usage: sleep x(in ms)\n");
        exit(1);
    }else{
        sleep(atoi(argv[1]));
    }
    exit(0);
    return 0;
}