#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"

int
main(int argc,char* argv[]){
    char buf[1024];
    char nowarg[1024];
    char *finalarg[MAXARG];
    int numarg=0,nowarglen=0,nowlen=0;
    for(int i=1;i<argc;i++){
        finalarg[numarg]=argv[i];
        numarg++;
        //printf("%s ",argv[i]);
    }
    //printf("\n");
    while((nowlen = read(0, buf, sizeof(buf)))>0){
        //printf("read: %s \n",buf);
        for(int i=0;i<nowlen;i++){
            if(buf[i]==' '){
                if(nowarglen==0) continue;
                nowarg[nowarglen++]=0;
                if(numarg==MAXARG){
                    fprintf(2,"xargs: too many arguments\n");
                    exit(1);
                }
                if(finalarg[numarg]) free(finalarg[numarg]);
                finalarg[numarg]=(char*) malloc(strlen(nowarg)*sizeof(char));
                strcpy(finalarg[numarg],nowarg);
                nowarglen=0;
                numarg++;
            }else if(buf[i]=='\n'){
                //printf("new line ended\n");
                if(nowarglen==0) continue;
                nowarg[nowarglen++]=0;
                if(numarg==MAXARG){
                    fprintf(2,"xargs: too many arguments\n");
                    exit(1);
                }
                if(finalarg[numarg]) free(finalarg[numarg]);
                finalarg[numarg]=(char*) malloc(strlen(nowarg)*sizeof(char));
                strcpy(finalarg[numarg],nowarg);
                numarg++;
                finalarg[numarg]=0;
                numarg++;
                nowarglen=0;
                // for(int j=0;j<numarg;j++){
                //     if(finalarg[j]) printf("%s ",finalarg[j]);
                //     else printf("NULL ");
                // }
                // printf("\n");
                if(fork()==0){
                    exec(finalarg[0],finalarg);
                    fprintf(2,"xargs: exec failed\n");
                    exit(1);
                }else{
                    numarg=argc-1;
                    wait(0);
                }
            }else{
                if(nowarglen+1==1024){
                    fprintf(2,"xargs: arguments too long,maxsize 1024\n");
                    exit(1);
                }
                nowarg[nowarglen++]=buf[i];
            }
        }
    }
    for(int i=0;i<MAXARG;i++){
        if(finalarg[i]) free(finalarg[i]);
    }
    exit(0);
}