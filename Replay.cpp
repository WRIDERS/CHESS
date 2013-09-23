#include <stdio.h>
#include "defs.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

void cleanup(){
	
	
	system(">|prevtrace");
	
}

void copy(char* source,char* target){
	char* cmd=(char*)malloc(sizeof(source)+sizeof(target)+5);
	sprintf(cmd,"cp %s %s",source,target);
	system(cmd);	
}


int main(int argc,char *argv[]){
	if(argc==1){
		printf("Please specify the replay source \n");
		return 0;
	}
	
	char* target=argv[1];
	char* replay=argv[2];
	char*  env[]={"LD_PRELOAD=./chess.so",NULL};

	int ReturnCode=0;
	int status=0;

	int NUMRUN=0;
	int NUMDEADLOCK=0;
	int NUMSEGFAULT=0;
	
	
	cleanup();
	copy(replay,"prevtrace");
	
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
	            printf("\t\t\t\t\tChild did not terminate with exit\n\n\n\n");
	            ReturnCode=-1;
            }
            if(ReturnCode==EXIT_STATUS_DEADLOCK_INTERLEAVING){            
            	printf("DEADLOCK\n");
            }
            if(ReturnCode==-1) {
            	printf("SEGFAULT\n");
            }
		}
			
	return 0;
}












