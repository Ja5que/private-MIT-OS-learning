#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
char *getfilename(char *path){
    char *p;
    for(p=path+strlen(path);p>=path&&*p!='/';p--);
    p++;
    return p;
}
void dfs(char *path,char *name){
    char buf[512],*p;
    int fd;
    struct stat st;
    struct dirent de;
    if((fd=open(path,0))<0){
        fprintf(2,"find: cannot open %s\n",path);
        return;
    }
    if(fstat(fd,&st)<0){
        fprintf(2,"find: cannot stat %s\n",path);
        return;
    }
    switch(st.type){
        case T_FILE:
            if(strcmp(getfilename(path),name)==0){
                printf("%s\n",path);
            }
            break;
        case T_DIR:          
            strcpy(buf,path);
            p=buf+strlen(buf);
            *p++='/';
            while(read(fd,&de,sizeof(de))==sizeof(de)){
                 if(strcmp(de.name,".")==0||
                strcmp(de.name,"..")==0)
                continue;  
                if(de.inum==0)
                    continue;
                memmove(p,de.name,DIRSIZ);
                p[DIRSIZ]=0;
                dfs(buf,name);
            }
            break;
    }
    close(fd);
    return;
}
int main(int argc,char *argv[]){
    if(argc==2){
        dfs(".",argv[1]);
    }else if (argc==3){
        dfs(argv[1],argv[2]);
    }
    else{
        exit(0);
        fprintf(2,"usage: find path\n");
    }
        exit(1);
}