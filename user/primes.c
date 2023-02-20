#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
//#include "stdio.h"

int main(int argc,char *argv[]){
    int oldp[2],newp[2];
   // int wlen=0;
   // char buf[1024];

    pipe(newp);
    if(fork()==0){
        int now=-1,cnt;
        int flagnew=0;
        memcpy(oldp,newp,2*sizeof(int));
        close(oldp[1]);
        while(read(oldp[0],&cnt,sizeof(cnt))>0){
            //printf("%d received %d\n",getpid(),cnt);
            if(now==-1){
                now=cnt;
                //printf("prime %d\n",now);
            }
            else if(cnt % now ==0) continue;
            else{
                if(flagnew==0){
                    pipe(newp);
                    if(fork()==0){
                        //printf("new process %d\n",getpid());
                        memcpy(oldp,newp,2*sizeof(int));
                        now=-1;
                        close(oldp[1]);
                    }else {
                        close(newp[0]);
                        flagnew=1;
                        write(newp[1],&cnt,sizeof(cnt));
                    }
                }else write(newp[1],&cnt,sizeof(cnt));
            }
        }
        printf("prime %d\n",now);
        close(newp[1]);
        close(newp[0]);
        if(flagnew==1) wait(0);
        close(oldp[1]);
        close(oldp[0]);
        //printf("process %d flag is %d\n",getpid(),flagnew);
    }else{
        close(newp[0]);
        for(int i=2;i<=35;i++){
            write(newp[1],&i,sizeof(i));
        }
        close(newp[1]);
        //printf("generate finish\n");
        wait(0);
    }
    //printf("process %d exit\n",getpid());
    exit(0);
}