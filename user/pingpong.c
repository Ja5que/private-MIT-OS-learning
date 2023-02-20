#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int 
main(int argc,char * argv[]){
    int p[2]; //0 for parent, 1 for child 
    pipe(p);
    if(fork()==0){
        if(read(p[0],"1",1)>0){
            printf("%d: received ping\n",getpid());
        }else exit(1);
        write(p[1],"1",1);
        close(p[1]);
        close(p[0]);
        exit(0);
    }else{
        write(p[1],"1",1);
        wait(0);
        if(read(p[0],"1",1)>0){
            printf("%d: received pong\n",getpid());
        }
        close(p[1]);
        close(p[0]);
    }
    exit(0);
}