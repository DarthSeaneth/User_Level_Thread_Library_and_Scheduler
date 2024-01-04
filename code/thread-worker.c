// File:	thread-worker.c

// List all group member's name: Fulton Wilcox III, Sean Patrick
// username of iLab: frw14, smp429
// iLab Server: ilab4
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <ucontext.h>
#include <sys/time.h>
#include <time.h>
#include "thread-worker.h"

#define STACK_SIZE SIGSTKSZ
#define DEBUG 0

//Global counter for total context switches and 
//average turn around and response time
long tot_cntx_switches=0;
double avg_turn_time=0;
double avg_resp_time=0;

double tot_resp_time=0;
double tot_turn_time=0;

// INITAILIZE ALL YOUR OTHER VARIABLES HERE
// YOUR CODE HERE

/* Robbie's variables */
int thread_counter = 0; //counts # of threads
tcb *threadQueue; //thread run queue (MLFQ level 1) (main queue for PSJF scheduling)
//int threadQueueSize = 0; 
ucontext_t scheduler; //context for scheduler
ucontext_t benchmark; //context for benchmarks
ucontext_t context_main; //context for main thread creation call
struct itimerval sched_timer; //timer
tcb *curThread; //currently running thread
int initialcall = 1;

/* Sean's variables */
/* MLFQ levels 2-4 */
tcb *threadQueue_level_2; 
tcb *threadQueue_level_3; 
tcb *threadQueue_level_4; 
/* Blocked thread queue */
tcb *blockedQueue; 
/* Terminated thread queue*/
tcb *terminatedQueue; 
/* How many quantums have elasped in total */
int total_quantums_elapsed;

/* Thread 1 is the main thread */

/* create a new thread */
int worker_create(worker_t * thread, pthread_attr_t * attr, 
                      void *(*function)(void*), void * arg) {

	// - create Thread Control Block (TCB)
	// - create and initialize the context of this worker thread
	// - allocate space of stack for this thread to run
	// after everything is set, push this thread into run queue and 
	// - make it ready for the execution.

	// YOUR CODE HERE
	
	//creates tcb, gets context, makes stack
	tcb* control_block = malloc(sizeof(tcb));
	create_tcb(thread, control_block, function, arg);
	threadQueue = enqueue(control_block, threadQueue);
	if(initialcall) {
		//create context for scheduler and benchmark program
		scheduler_benchmark_create_context();
	}
    return 0;
};

/* give CPU possession to other user-level worker threads voluntarily */
int worker_yield() {
	
	// - change worker thread's state from Running to Ready
	// - save context of this thread to its thread control block
	// - switch from thread context to scheduler context

	if(curThread != NULL) {
		if(DEBUG) printf("yielding...");
		curThread->thread_status = ready;
		tot_cntx_switches++;
		swapcontext(&curThread->context, &scheduler);
	}
	
	return 0;
};

/* terminate a thread */
void worker_exit(void *value_ptr) {
	if(DEBUG) printf("Thread %d terminating\n", curThread->thread_id);
	disable_timer();
	curThread->thread_status = terminated;
	if(value_ptr) curThread->return_value = value_ptr;
	if(!curThread->end_time) curThread->end_time = clock();

	curThread->response_time = ((curThread->start_time - curThread->queued_time) * 1000)/CLOCKS_PER_SEC;
	curThread->turnaround_time = ((curThread->end_time - curThread->queued_time) * 1000)/CLOCKS_PER_SEC;

	tot_resp_time += curThread->response_time;
	tot_turn_time += curThread->turnaround_time;
	avg_resp_time = tot_resp_time/thread_counter;
	avg_turn_time = tot_turn_time/thread_counter;

	if(DEBUG)printf("\nresponse: %ld\nturnaround: %ld\n", curThread->response_time, curThread->turnaround_time);
};


/* Wait for thread termination */
int worker_join(worker_t thread, void **value_ptr) {
	// - wait for a specific thread to terminate
	// - de-allocate any dynamic memory created by the joining thread

	// YOUR CODE HERE

	tcb* joining_thread = searchAllQueues(thread);
	if(joining_thread == NULL){ if(DEBUG) printf("join error: search returned null\n"); exit(1);}
	if(DEBUG)printf("found %d, searched with %d\n", joining_thread->thread_id, thread);

	while(joining_thread->thread_status != terminated) {}

	if(value_ptr) *value_ptr = joining_thread->return_value; //save return value
	if(joining_thread->stack) free(joining_thread->stack); //free thread memory
	free(joining_thread);

	return 0;
};

/* initialize the mutex lock */
int worker_mutex_init(worker_mutex_t *mutex, 
                          const pthread_mutexattr_t *mutexattr) {
	//- initialize data structures for this mutex

	// YOUR CODE HERE

	if(mutex == NULL) return -1;
	if(mutex->initialized == 1) return -1;

	mutex->initialized = 1;
	mutex->locked = 0;
	mutex->lock_owner = NULL;
	if(DEBUG) printf("mutex initialized\n");

	return 0;
};

/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex) {

    // - use the built-in test-and-set atomic function to test the mutex
    // - if the mutex is acquired successfully, enter the critical section
    // - if acquiring mutex fails, push current thread into block list and
    // context switch to the scheduler thread

    // YOUR CODE HERE
	if(mutex == NULL){ if(DEBUG) printf("mutex is null\n"); return -1; }
	if(mutex->initialized == 0){ if(DEBUG) printf("mutex uninitialized\n"); return -1; }

	//will return initial value of mutex->locked
	while(__atomic_test_and_set(&mutex->locked, 1)){ //if lock was acquired by another thread
		if(DEBUG) printf("blocking thread %d\n", curThread->thread_id);
		curThread->thread_status = blocked;
		tot_cntx_switches++;
		swapcontext(&curThread->context, &scheduler);
	}
	
	//if lock can be acquired, keep track of lock owner
	mutex->lock_owner = curThread;
	//printf("locking mutex\n");

    return 0;
};

/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex) {
	// - release mutex and make it available again. 
	// - put threads in block list to run queue 
	// so that they could compete for mutex later.
	
	// YOUR CODE HERE

	if(mutex == NULL){ if(DEBUG) printf("mutex is null\n"); return -1; }
	if(mutex->initialized == 0 || mutex->locked == 0){ if(DEBUG) printf("mutex unlocked or uninitialized\n"); return -1; }
	if(mutex->lock_owner != curThread){ if(DEBUG) printf("access denied\n"); return -1; }

	//remove thread from head of blocked queue and add to thread queue
	blockedDequeue();

	//release the lock
	mutex->locked = 0;
	mutex->lock_owner = NULL;
	//printf("unlocking mutex\n");

	return 0;
};


/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex) {
	// - de-allocate dynamic memory created in worker_mutex_init

	//check for valid mutex and lock status
	if(mutex == NULL) {if(DEBUG)printf("mutex is null\n"); return -1;}
	if(mutex->initialized == 0 || mutex->locked == 1) {if(DEBUG)printf("mutex locked or uninitialized\n"); return -1;}

	if(DEBUG) printf("destroying mutex\n");
	mutex->initialized = 0;
	mutex->locked = 0;
	mutex->lock_owner = NULL;

	return 0;
};

/* scheduler */
static void schedule() {
	// - every time a timer interrupt occurs, your worker thread library 
	// should be contexted switched from a thread context to this 
	// schedule() function

	// - invoke scheduling algorithms according to the policy (PSJF or MLFQ)

	// if (sched == PSJF)
	//		sched_psjf();
	// else if (sched == MLFQ)
	// 		sched_mlfq();

	// YOUR CODE HERE

	if(DEBUG) printf("inside scheduler\n");
	while(!areQueuesEmpty()) {
		disable_timer();
		// - schedule policy
		#ifndef MLFQ
			sched_psjf();
		#else 
			sched_mlfq();
		#endif
		if(!curThread->start_time) curThread->start_time = clock();
		if(DEBUG) printf("swapping to thread %d\n", curThread->thread_id);
		curThread->thread_status = running;
		enable_timer();
		if(curThread != NULL) { 
			tot_cntx_switches++;
			swapcontext(&scheduler, &curThread->context);
			curThread->quantums_elapsed++;
		}
		if(DEBUG) {printf("thread queue: "); printQueue(threadQueue);}
		if(curThread->thread_status != terminated && curThread->thread_status != blocked){ 
			curThread->thread_status = ready;
			#ifndef MLFQ 
				threadQueue = enqueue(curThread, threadQueue);
			#else
				enqueueMLFQ(curThread);
			#endif
		}
		else if(curThread->thread_status == blocked){
			blockedQueue = enqueue(curThread, blockedQueue);
			if(DEBUG) {printf("blocked queue: "); printQueue(blockedQueue);}
		}
		else if(curThread->thread_status == terminated){
			terminatedQueue = enqueue(curThread, terminatedQueue);
			if(DEBUG) {printf("terminated queue: "); printQueue(terminatedQueue);}
		}
	}
}

/* Pre-emptive Shortest Job First (POLICY_PSJF) scheduling algorithm */
static void sched_psjf() {
	// - your own implementation of PSJF
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
	threadQueue = dequeuePSJF(threadQueue);
}


/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq() {
	// - your own implementation of MLFQ
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
	dequeueMLFQ();
}

//DO NOT MODIFY THIS FUNCTION
/* Function to print global statistics. Do not modify this function.*/
void print_app_stats(void) {

       fprintf(stderr, "Total context switches %ld \n", tot_cntx_switches);
       fprintf(stderr, "Average turnaround time %lf \n", avg_turn_time);
       fprintf(stderr, "Average response time  %lf \n", avg_resp_time);
}


// Feel free to add any other functions you need

// YOUR CODE HERE

static void create_tcb(worker_t * thread, tcb* control_block, void *(*function)(void*), void * arg) {
	if (getcontext(&control_block->context) < 0){
		perror("getcontext");
		exit(1);
	}
	void *stack=malloc(STACK_SIZE);
	if (stack == NULL){
		perror("Failed to allocate stack");
		exit(1);
	}

	//stack and context
	control_block->context.uc_link=&scheduler;
	control_block->context.uc_stack.ss_sp=stack;
	control_block->context.uc_stack.ss_size=STACK_SIZE;
	control_block->context.uc_stack.ss_flags=0;
	control_block->stack = stack;

	//other attributes
	control_block->thread_id = thread_counter;
	*thread = control_block->thread_id;
	thread_counter++;
	control_block->thread_status = ready;
	control_block->priority = 0; 
	control_block->quantums_elapsed = 0;
	makecontext(&control_block->context,(void *)function, 1, arg);
}

tcb* enqueue(tcb *thread, tcb *queue) {
	if(queue == NULL) queue = thread;
	else {
		tcb *temp = queue;
		while(temp->next != NULL) temp = temp->next;
		temp->next = thread;
	}
	thread->next = NULL;
	if(!thread->queued_time) thread->queued_time = clock();
	if(DEBUG)printf("\ntime is %ld \n", thread->queued_time);
	return queue;
}

void enqueueMLFQ(tcb* thread){
	switch(thread->priority){
		case(0):
			threadQueue = enqueue(thread, threadQueue);
			break;
		case(1):
			threadQueue_level_2 = enqueue(thread, threadQueue_level_2);
			break;
		case(2):
			threadQueue_level_3 = enqueue(thread, threadQueue_level_3);
			break;
		case(3):
			threadQueue_level_4 = enqueue(thread, threadQueue_level_4);
			break;
		default:
			threadQueue = enqueue(thread, threadQueue);
			break;
	}
}

tcb* dequeue(tcb *queue) {
	curThread = queue;
	queue = queue->next;
	return queue;
}

tcb* dequeuePSJF(tcb *queue){
	if(isEmpty(queue)) return NULL;
	tcb* target = queue; //head of queue
	tcb* cur = queue;
	while(cur != NULL){ //find thread with lowest quantums elapsed
		if(cur->quantums_elapsed < target->quantums_elapsed) target = cur;
		cur = cur->next;
	}
	if(target == queue){ //if only 1 thread in queue or if head had lowest quantums elapsed
		queue = dequeue(queue);
		return queue;
	}
	curThread = target;
	tcb* prev = queue;
	//find thread before target thread in queue
	while(prev->next != target){
		prev = prev->next;
	}
	//remove target thread from queue
	prev->next = target->next;
	return queue;
}

void dequeueMLFQ(){
	if(!isEmpty(threadQueue)) threadQueue = dequeue(threadQueue);
	else if(!isEmpty(threadQueue_level_2)) threadQueue_level_2 = dequeue(threadQueue_level_2);
	else if(!isEmpty(threadQueue_level_3)) threadQueue_level_3 = dequeue(threadQueue_level_3);
	else if(!isEmpty(threadQueue_level_4)) threadQueue_level_4 = dequeue(threadQueue_level_4);
}

void blockedDequeue(){
	if(!isEmpty(blockedQueue)){
		if(DEBUG) printf("removing thread from blocked queue\n");
		tcb* temp = blockedQueue;
		blockedQueue = blockedQueue->next;
		temp->thread_status = ready;
		threadQueue = enqueue(temp, threadQueue);
		if(DEBUG) {printf("thread queue: "); printQueue(threadQueue);}
	}
}

void resetMLFQ(){
	//set all thread priorities to 0 and enqueue to top-most queue (threadQueue)
	if(curThread != NULL) curThread->priority = 0;
	tcb* blockedTemp = blockedQueue;
	while(blockedTemp != NULL){
		blockedTemp->priority = 0;
		blockedTemp = blockedTemp->next;
	}
	tcb* temp = threadQueue_level_2;
	while(threadQueue_level_2 != NULL){
		temp->priority = 0;
		threadQueue_level_2 = threadQueue_level_2->next;
		threadQueue = enqueue(temp, threadQueue);
		temp = threadQueue_level_2;
	}
	temp = threadQueue_level_3;
	while(threadQueue_level_3 != NULL){
		temp->priority;
		threadQueue_level_3 = threadQueue_level_3->next;
		threadQueue = enqueue(temp, threadQueue);
		temp = threadQueue_level_3;
	}
	temp = threadQueue_level_4;
	while(threadQueue_level_4 != NULL){
		temp->priority = 0;
		threadQueue_level_4 = threadQueue_level_4->next;
		threadQueue = enqueue(temp, threadQueue);
		temp = threadQueue_level_4;
	}
}

static tcb* search(worker_t thread, tcb* queue) {
	tcb* temp = queue;
	while(temp != NULL) {
		if(temp->thread_id == thread) return temp;
		temp = temp->next;
	}
	return NULL;
}

static tcb* searchAllQueues(worker_t thread){
	tcb *joining_thread = search(thread, threadQueue);
	if(joining_thread == NULL) joining_thread = search(thread, blockedQueue);
	if(joining_thread == NULL) joining_thread = search(thread, terminatedQueue);
	#ifdef MLFQ
		if(joining_thread == NULL) joining_thread = search(thread, threadQueue_level_2);
		if(joining_thread == NULL) joining_thread = search(thread, threadQueue_level_3);
		if(joining_thread == NULL) joining_thread = search(thread, threadQueue_level_4);
	#endif
	return joining_thread;
}

int isEmpty(tcb *queue) {
	return queue == NULL;
}

int areQueuesEmpty(){
	#ifndef MLFQ 
		if(isEmpty(threadQueue)) return 1;
	#else
		if(isEmpty(threadQueue) && isEmpty(threadQueue_level_2) && isEmpty(threadQueue_level_3) && isEmpty(threadQueue_level_4)) return 1;
	#endif
	return 0;
}

void printQueue(tcb *queue) {
	tcb *temp = queue;
	while(temp != NULL) {
		printf("thread %d, ", temp->thread_id);
		temp = temp->next;
	}
	printf("\n\n");
}

void toString(tcb *thread) {
	printf("Thread id: %d\nStatus: %d\n\n", thread->thread_id, thread->thread_status);
}

static void signal_handler(int signum) {
	if(DEBUG) puts("signal received\n");

	if(curThread != NULL ){
		#ifdef MLFQ 
			if(curThread->priority < 3) curThread->priority++;
			if(++total_quantums_elapsed >= S){
				total_quantums_elapsed = 0;
				resetMLFQ();
			}
		#endif
		tot_cntx_switches++;
		swapcontext(&curThread->context, &scheduler);
	}
}

static void enable_timer() {
	sched_timer.it_interval.tv_usec = TIME_US; 
	sched_timer.it_interval.tv_sec = TIME_S;

	sched_timer.it_value.tv_usec = TIME_US;
	sched_timer.it_value.tv_sec = TIME_S;
	setitimer(ITIMER_PROF, &sched_timer, NULL);
}

static void disable_timer() {
	sched_timer.it_interval.tv_usec = 0; 
	sched_timer.it_interval.tv_sec = 0;

	sched_timer.it_value.tv_usec = 0;
	sched_timer.it_value.tv_sec = 0;
	setitimer(ITIMER_PROF, &sched_timer, NULL);
}

void setup_timer() {
	// Use sigaction to register signal handler
	struct sigaction sa;
	memset (&sa, 0, sizeof (sa));
	sa.sa_handler = &signal_handler;
	sigaction (SIGPROF, &sa, NULL);

	// Set up what the timer should reset to after the timer goes off

	sched_timer.it_interval.tv_usec = TIME_US; 
	sched_timer.it_interval.tv_sec = TIME_S;

	sched_timer.it_value.tv_usec = TIME_US;
	sched_timer.it_value.tv_sec = TIME_S;

	// Set the timer up (start the timer)
	setitimer(ITIMER_PROF, &sched_timer, NULL);
}

int scheduler_benchmark_create_context() {
	initialcall = 0;
	getcontext(&scheduler);
	void* stack = malloc(SIGSTKSZ);
	scheduler.uc_link=NULL;
	scheduler.uc_stack.ss_sp=stack;
	scheduler.uc_stack.ss_size=STACK_SIZE;
	scheduler.uc_stack.ss_flags=0;
	if(DEBUG) printf("scheduler/benchmark context created\n");

	makecontext(&scheduler, (void *)&schedule, 0, NULL);
	setup_timer();

	getcontext(&context_main);

	tcb *mainTCB = malloc(sizeof(tcb));

	//other attributes
	mainTCB->thread_id = thread_counter;
	thread_counter++;
	mainTCB->thread_status = ready;
	mainTCB->priority = 0; //don't know what to do with this, we're not there yet
	mainTCB->quantums_elapsed = 0;
	threadQueue = enqueue(mainTCB, threadQueue);
	tot_cntx_switches++;
	swapcontext(&mainTCB->context, &scheduler);
	return 0;
}

