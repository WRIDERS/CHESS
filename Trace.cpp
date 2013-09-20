#include <stdio.h>
#include <fstream>
#include <limits>


#define PHASE_EMBRYO 0x00
#define PHASE_REPLAY 0x11
#define PHASE_RECORD 0x22


#define EXIT_STATUS_COMPLETED_EXPLORATION 0x9
#define EXIT_STATUS_INVALID_INTERLEAVING 0x10
#define EXIT_STATUS_DEADLOCK_INTERLEAVING 0x11

using namespace std;

static int  PHASE=PHASE_EMBRYO;
static int 	LEVEL=1;

const char tracefilename[]="prevtrace";

static unsigned* SCHEDULE=NULL;
static unsigned REPLAYLEVEL=0;


unsigned countlines(const char* file){
	std::ifstream infile(file,ios::in);
	unsigned NUM=0;
	if(infile!=NULL){
		for(std::string line; getline(infile,line);)
			NUM++;
			
		infile.close();	
	}
	return NUM;
}

std::fstream& GotoLine(std::fstream& file, unsigned int num){
    file.seekg(std::ios::beg);
    for(int i=0; i < num - 1; ++i){
        file.ignore(std::numeric_limits<std::streamsize>::max(),'\n');
    }
    return file;
}

void clearFile(const char* filename){
	std::fstream file;
	file.open(filename,std::fstream::out | std::fstream::trunc);
	file.close();
}

void initTracer(){
	unsigned NUMLINES=countlines(tracefilename);
	
	if(NUMLINES==0){
		std::ofstream createfile(tracefilename,ios::out);
		createfile.close();
		printf("Trace not present\n");
		return;
	}
	
	std::ifstream infile(tracefilename,ios::in);

	unsigned totalchoice,choice;
	
	SCHEDULE=(unsigned*)malloc(NUMLINES*sizeof(unsigned));
	unsigned i=0;
	int lastlevel=-1;
	
	while(infile>>choice>>totalchoice){
		SCHEDULE[i]=choice;
		if(choice!=totalchoice){
			lastlevel=i;
		}
		i++;		
	}
	
	if(lastlevel==-1){
		//Exploration complete;
		printf("EXPLORATION IS COMPLETED \n \n \n");
		exit(EXIT_STATUS_COMPLETED_EXPLORATION);
	}
	else{
		REPLAYLEVEL=lastlevel+1;
		SCHEDULE[lastlevel]++;
		printf("REPLAYLEVEL %d CHOICE %d \n\n",REPLAYLEVEL,SCHEDULE[lastlevel]);
		clearFile(tracefilename);		
	}
	
	PHASE=PHASE_REPLAY;	
}


unsigned getCurrentPhase(){
	return PHASE;	
}

void incrementLevel(){
	LEVEL++;
	if(LEVEL>REPLAYLEVEL){
		PHASE=PHASE_RECORD;
	}
}

void logChoice(unsigned choice,unsigned totalchoice){
	printf("LOG CALLED %d %d %d \n\n",choice,totalchoice,LEVEL);
	std::fstream outfile(tracefilename);
	GotoLine(outfile,LEVEL);
	outfile << choice <<" "<< totalchoice << endl;
	outfile.close();
}


unsigned getNextThread(){
	if(LEVEL<=REPLAYLEVEL && PHASE==PHASE_REPLAY)
		return SCHEDULE[LEVEL-1];
	else 
		return 0;			
}








