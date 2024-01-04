// File:	worker_t.h

// List all group member's name: Fulton Wilcox III, Sean Patrick
// username of iLab: frw14, smp429
// iLab Server: ilab4

#ifndef WORKER_T_H
#define WORKER_T_H

#define _GNU_SOURCE

/* To use Linux pthread Library in Benchmark, you have to comment the USE_WORKERS macro */
#define USE_WORKERS 1

/* Determines how long the timer runs before swapping to scheduler context (10ms) */
#define QUANTUM 10
#define TIME_S QUANTUM / 1000
#define TIME_US (QUANTUM * 1000) % 1000000

/* How many times quantums must elapse before resetting MLFQ */
#define S 10

/* include lib header files that you need here: */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <time.h>

typedef uint worker_t;
typedef enum status {ready, running, blocked, terminated} status;

typedef struct TCB {
	/* add important states in a thread control block */
	// thread Id
	// thread status
	// thread context
	// thread stack
	// thread priority
	// And more ...

	// YOUR CODE HERE
	worker_t thread_id;
	status thread_status;
	ucontext_t context;
	void* stack;
	int priority;
	int quantums_elapsed;
	struct TCB* next;
	void* return_value;
	clock_t queued_time;
	clock_t start_time;
	clock_t end_time;
	long response_time;
	long turnaround_time;

} tcb; 

/* mutex struct definition */
typedef struct worker_mutex_t {
	/* add something here */

	// YOUR CODE HERE
	volatile int initialized; //is mutex initialized
	volatile int locked; //is mutex lock currently locked
	tcb* lock_owner; //pointer to TCB owner of current mutex lock

} worker_mutex_t;

/* define your data structures here: */
// Feel free to add your own auxiliary data structures (linked list or queue etc...)

// YOUR CODE HERE


/* Function Declarations: */

/* create a new thread */
int worker_create(worker_t * thread, pthread_attr_t * attr, void
    *(*function)(void*), void * arg);

/* give CPU pocession to other user level worker threads voluntarily */
int worker_yield();

/* terminate a thread */
void worker_exit(void *value_ptr);

/* wait for thread termination */
int worker_join(worker_t thread, void **value_ptr);

/* initial the mutex lock */
int worker_mutex_init(worker_mutex_t *mutex, const pthread_mutexattr_t
    *mutexattr);

/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex);

/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex);

/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex);

/* Function to print global statistics. Do not modify this function.*/
void print_app_stats(void);

static void sched_psjf();
static void sched_mlfq();


//---------------------Util methods we made-------------------------// 
/* Signal handler for timer interrupts */
static void signal_handler();

/* Enables signal handler timer */
static void enable_timer();

/* Disables signal handler timer*/
static void disable_timer();

/* Initialize thread control block data for worker thread */
static void create_tcb(worker_t * thread, tcb* control_block, void *(*function)(void*), void * arg);

/* Search queue for specified thread */
static tcb* search(worker_t thread, tcb* queue);

/* Search all queues for specified thread */
static tcb* searchAllQueues(worker_t thread);

/* Function to add new thread to queue.*/
tcb* enqueue(tcb *thread, tcb *queue);

/* Function to add thread to appropriate level of MLFQ */
void enqueueMLFQ(tcb* thread);

/* Function to remove thread (RR scheduling).*/
tcb* dequeue(tcb *queue);

/* Function to remove thread with PSJF scheduling */
tcb* dequeuePSJF(tcb *queue);

/* Function to remove thread with MLFQ scheduling */
void dequeueMLFQ();

/* Removes thread from blocked queue and enqueues to run queue */
void blockedDequeue();

/* Function to reset MLFQ and thread priorities */
void resetMLFQ();

/* Checks if specified queue is empty */
int isEmpty(tcb *threadQueue);

/* Checks if entire MLFQ is empty */
int areQueuesEmpty();

/* Function to print thread queue.*/
void printQueue(tcb *queue);

/* Function to print information about a thread.*/
void toString(tcb *thread);

/* Makes context for scheduler thread.*/
int scheduler_benchmark_create_context();

//------------------------------------------------------------------// 


#ifdef USE_WORKERS
#define pthread_t worker_t
#define pthread_mutex_t worker_mutex_t
#define pthread_create worker_create
#define pthread_exit worker_exit
#define pthread_join worker_join
#define pthread_mutex_init worker_mutex_init
#define pthread_mutex_lock worker_mutex_lock
#define pthread_mutex_unlock worker_mutex_unlock
#define pthread_mutex_destroy worker_mutex_destroy
#endif

#endif

