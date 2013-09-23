#include <stdio.h>
#include "defs.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>


void cleanup(){
	
	system("rm -rf DEADLOCK");
	system("mkdir DEADLOCK");
	
	system("rm -rf SEGFAULT");
	system("mkdir SEGFAULT");
	
	system(">|prevtrace");
	system(">|replaytrace");
}

void copy(char* source,char* target){
	char* cmd=(char*)malloc(sizeof(source)+sizeof(target)+5);
	sprintf(cmd,"cp %s %s",source,target);
	system(cmd);	
}


int main(int argc,char *argv[]){
	if(argc==1){
		printf("Please specify the target program\n");
		return 0;
	}
	
	char* target=argv[1];
	char*  env[]={"LD_PRELOAD=./chess.so",NULL};

	int ReturnCode=0;
	int status=0;

	int NUMRUN=0;
	int NUMDEADLOCK=0;
	int NUMSEGFAULT=0;
	
	cleanup();
	
	while(ReturnCode!=EXIT_STATUS_COMPLETED_EXPLORATION ){
	
		printf("\t\t\t\t RUN NUMBER %d \n\n",NUMRUN);
		copy("prevtrace","replaytrace");
		pid_t pid=fork();
		if(pid<0) {
			printf("forking failed\n");
			return 0;
		}
		if(pid==0){
			exit(execve(target,NULL,env));
		}
		else{
			waitpid(pid,&status,0);
			
			if(WIFEXITED(status)){
            	//printf("Child's exit code %d\n", WEXITSTATUS(status));
            	ReturnCode=WEXITSTATUS(status);
            }
	        else{
	            printf("\t\t\t\t\tChild did not terminate with exit\n\n");
	            ReturnCode=-1;
            }
            if(ReturnCode==EXIT_STATUS_DEADLOCK_INTERLEAVING){
            	NUMDEADLOCK++;     
            	char* target= (char*)malloc(20);       
            	sprintf(target,"DEADLOCK/%d",NUMDEADLOCK);
            	copy("replaytrace",target);
            }
            if(ReturnCode==-1) {
            	NUMSEGFAULT++;
            	char* target= (char*)malloc(20);       
            	sprintf(target,"SEGFAULT/%d",NUMSEGFAULT);
            	copy("replaytrace",target);            	
            }
		}
		NUMRUN++;
	}
	
	printf("\n\n\n\n");
	printf("NUMBER OF INTERLEAVING EXPLORED = %d \n",NUMRUN);
	printf("NUMBER OF DEADLOCKS FOUND =%d \n ",NUMDEADLOCK);
	printf("NUMBER OF SEG FAULT FOUND = %d \n",NUMSEGFAULT);
	
	return 0;
}












