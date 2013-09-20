#include <stdlib.h>
#include <pthread.h>
#include <dlfcn.h>
#include <stdio.h>

#include "map.cpp"
#include "Trace.cpp"

#define MALLOC(type) (type*)calloc(1,sizeof(type))
#define ASSERT(cond,msg) if(cond) printf(msg)
#define PRINT(str,arg) if(1) printf(str,arg)

using namespace std;

#define PTHREAD_WAITING_THREAD 0x222
#define PTHREAD_ACTIVE_THREAD 0x333
#define PTHREAD_DEAD_THREAD 0x444

typedef long unsigned addr_type;

int (*original_pthread_create)(pthread_t*, const pthread_attr_t*, void* (*)(void*), void*) = NULL;
int (*original_pthread_mutex_lock)(pthread_mutex_t*) = NULL;
int (*original_pthread_join)(pthread_t, void**) = NULL;
int (*original_pthread_mutex_unlock)(pthread_mutex_t*) = NULL;

void init (void);
void sched( void );
void findnextthreadtoschedule(pthread_t pred_thread);

static bool initialized=false;
static int THREAD_COUNT=0;
static int LOCK_COUNT=0;
static bool PANIC_KILL_SIGNAL=false;
static int EXIT_STATUS_IF_KILL=0x1;

struct thread_data{
	pthread_t thread_id;
	unsigned  local_thread_id;
	unsigned  state;
	pthread_mutex_t* waiting_lock;
};

pthread_mutex_t SCHED_LOCK=PTHREAD_MUTEX_INITIALIZER;

map<pthread_t,struct thread_data*> all_thread;
map<pthread_mutex_t*,pthread_t> lock_map;

pthread_t current_thread=NULL;
pthread_t next_thread=NULL;

struct Thread_Arg{
	void* (*start_routine)(void*);
	void* arg;
};


unsigned getActiveThreadNum(){

	struct map<pthread_t,thread_data*>::map_data* start=all_thread.ROOT;
	unsigned NUM=0;
    while(start!=NULL){
    	struct thread_data* thread_data=start->data;
    	if(thread_data->state==PTHREAD_ACTIVE_THREAD)
    		NUM++;
    	start=start->next;
    }
    return NUM;	
}



void* closure_wrapper(void* arg){

	struct Thread_Arg* th_arg=(struct Thread_Arg*)arg;

	original_pthread_mutex_lock(&SCHED_LOCK);
	
	PRINT("NEW THREAD %lx BEGINS\n",pthread_self());
	void* ret=th_arg->start_routine(th_arg->arg);
	//free(th_arg);
	
	struct thread_data* cur_thread_data=(struct thread_data*)all_thread.get(pthread_self());
	ASSERT((cur_thread_data==NULL),"SNAP!!");
	cur_thread_data->state=PTHREAD_DEAD_THREAD;

	findnextthreadtoschedule(NULL);
	
	original_pthread_mutex_unlock(&SCHED_LOCK);
	PRINT("EXITING THREAD %x\n",pthread_self());
	return(ret);
}


int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void*), void *arg)
{
    init();    
    printf("Thread Creation no %d \n",THREAD_COUNT);
	THREAD_COUNT++;		

	struct Thread_Arg th_arg;
	
	th_arg.start_routine=start_routine;
	th_arg.arg=arg;

    int ret = original_pthread_create(thread, attr, closure_wrapper, (void*)&th_arg);
	
	struct thread_data* new_thread = MALLOC(struct thread_data);
	new_thread->thread_id=*thread;
	new_thread->local_thread_id=THREAD_COUNT;
	new_thread->state=PTHREAD_ACTIVE_THREAD;

	all_thread.put(*thread,new_thread);
	
	//Set next thread to newly created thread 
	//Execute newly created thread instantenously
	
	next_thread=*thread;
	
	sched();
    
    // TODO
    return ret;
}


bool mutex_lock_validity(pthread_mutex_t* arg)
{
	pthread_t holding_thread = lock_map.get(arg);
	if(holding_thread==NULL){
		return true;
	}
	else{// Do something if other thread is holding lock
		struct thread_data* cur_thread_data=all_thread.get(pthread_self());
		cur_thread_data->state=PTHREAD_WAITING_THREAD;
		cur_thread_data->waiting_lock=arg;
		return false;
	}
}

int pthread_mutex_lock(pthread_mutex_t *mutex){
	//    init();
	//This is a non deterministic point . Capture non-determinism here 
	findnextthreadtoschedule(NULL);

	// Call scheduler and wait for your turn
	sched();

	//I am not worthy of this lock
	while(!mutex_lock_validity(mutex)){
		findnextthreadtoschedule(pthread_self());
		sched();
	}

	logChoice(all_thread.indexnum(pthread_self()),getActiveThreadNum());	
	incrementLevel();
	
	PRINT("ACQUIRING LOCK %lx \n",mutex);		
	lock_map.put(mutex,(pthread_self()));
	return original_pthread_mutex_lock(mutex);
    // TODO
}




void findnextthreadtoschedule(pthread_t pred_thread){

	unsigned next_thread_id = getNextThread();
	
	//Try Replay
	if(next_thread_id!=0 && pred_thread==NULL){
		next_thread=all_thread.access(next_thread_id)->thread_id;
	}
	else if(pred_thread!=NULL){//Discovery Phase	
		
		struct map<pthread_t,thread_data*>::map_data* start=all_thread.ROOT;
		
		bool SEEK=false;		
		next_thread=NULL;
		
		while(start!=NULL ){
			struct thread_data* thread_data=start->data;
			
			if(pthread_equal(pred_thread,thread_data->thread_id)!=0 && !SEEK)
				SEEK=true;
			else if(SEEK && thread_data->state==PTHREAD_ACTIVE_THREAD ){
				next_thread=thread_data->thread_id;
				break;
			}
			start=start->next;				
		}
		
		//Exhausted myself
		if(next_thread==NULL){
			//Replay Phase
			if(next_thread_id!=0){
				//kill and do not create more logs
				printf("\n\n\n THIS INTERLEAVING IS INVALID \n\n\n");
				EXIT_STATUS_IF_KILL=EXIT_STATUS_INVALID_INTERLEAVING;
				PANIC_KILL_SIGNAL=true;				
			}
			else{//Discovery Phase
				//DeadLock phase
				//kill yourself and report bug
				printf("DEADLOCK ENCOUNTERED\n");
				EXIT_STATUS_IF_KILL=EXIT_STATUS_DEADLOCK_INTERLEAVING;
				PANIC_KILL_SIGNAL=true;
			}
		}
	}
	else if(pred_thread==NULL && next_thread_id==0){
		struct map<pthread_t,thread_data*>::map_data* start=all_thread.ROOT;
		while(start!=NULL ){
			struct thread_data* thread_data=start->data;
			
			if(thread_data->state==PTHREAD_ACTIVE_THREAD ){
				next_thread=thread_data->thread_id;
				break;
			}
			start=start->next;				
		}
	}
}

//Waiting point for all threads 
void sched(){
	PRINT("Inside sched %lx \n",pthread_self());
	original_pthread_mutex_unlock(&SCHED_LOCK);
	while(pthread_equal(pthread_self(),next_thread)==0 && !PANIC_KILL_SIGNAL);
	if(PANIC_KILL_SIGNAL) exit(EXIT_STATUS_IF_KILL);
	original_pthread_mutex_lock(&SCHED_LOCK);
	PRINT("Outside sched %lx \n",pthread_self());
}


int pthread_join(pthread_t joinee, void **retval)
{
	PRINT("THREAD JOIN\n",NULL);
    // TODO

    struct thread_data* join_thread_data=all_thread.get(joinee);
//    cur_thread_data->state=PTHREAD_WAITING_THREAD;

	while(join_thread_data->state!=PTHREAD_DEAD_THREAD){
		next_thread=joinee;
	    sched();
    }
    //original_pthread_mutex_unlock(&SCHED_LOCK);
    return original_pthread_join(joinee, retval);
    //original_pthread_mutex_lock(&SCHED_LOCK);
    //cur_thread_data->state=PTHREAD_ACTIVE_THREAD;
    //findnextthreadtoschedule(NULL);
	//sched();	
	
}



//No need to relnquish lock on unlock 
int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    // TODO
    //Mark all threads waiting on this lock as active 
	struct map<pthread_t,thread_data*>::map_data* start=all_thread.ROOT;
	
    while(start!=NULL){
    	struct thread_data* thread_data=start->data;
    	if(thread_data->state==PTHREAD_WAITING_THREAD && thread_data->waiting_lock==mutex)
    		thread_data->state=PTHREAD_ACTIVE_THREAD;
    	start=start->next;
    }
    
	lock_map.remove(mutex);
	PRINT("UNLOCK %lx \n",mutex);
    return original_pthread_mutex_unlock(mutex);
}



void 
init(void){
	if(!initialized){
		initTracer();
		original_pthread_create=( int(*)(pthread_t*, const pthread_attr_t*, void* (*)(void*), void*) )dlsym(RTLD_NEXT,"pthread_create");
		original_pthread_mutex_lock =(int (*)(pthread_mutex_t*))dlsym(RTLD_NEXT, "pthread_mutex_lock");
		original_pthread_mutex_unlock =(int (*)(pthread_mutex_t*))dlsym(RTLD_NEXT, "pthread_mutex_unlock");
        original_pthread_join = (int (*)(pthread_t, void**))dlsym(RTLD_NEXT, "pthread_join");
		initialized=true;
		
		THREAD_COUNT++;
		
		//insert main thread in the list
		struct thread_data* main_thread=MALLOC(struct thread_data);
		main_thread->thread_id=pthread_self();
		main_thread->state=PTHREAD_ACTIVE_THREAD;
		main_thread->local_thread_id=THREAD_COUNT;

		all_thread.comparator=pthread_equal;
		all_thread.put(pthread_self(),main_thread);

		original_pthread_mutex_lock(&SCHED_LOCK);				
	}
}


