# User_Level_Thread_Library_and_Scheduler
Sean Patrick

1)
API Implementation:

int worker_create(worker_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg):

    The job of worker_create is to create the tcb for a worker thread and initialize the main context and scheduler context if it is the first call of the session.
    The first thing that is done is allocate the memory for the tcb, call create_tcb which initializes its fields and makes it ready to run, and add it to the runqueue.
    If this is the first call of worker_create of the session, there will also be a call to create the scheduler and main benchmark threads. 

int worker_yield():

    worker_yield turns over control of the process to the scheduler, taking away control from the currently running thread.  The tcb's status field is changed back to 
    "ready" from "running", and the context is swapped back to the scheduler context.

void worker_exit(void *value_ptr):

    worker_exit is the mechanism by which a thread should terminate according to the framework of this library.  The argument, a void pointer, is passed in to store the return
    value of the function if there is one.  the status of the thread is changed to "terminated" from "running" and the time is recorded and saved in the tcb for the app statistics.
    The turnaround and response time is also recorded, and the average response and turnaround time updated.  

int worker_join(worker_t thread, void **value_ptr):

    worker_join blocks the calling thread until the thread whose thread id is passed into the function terminates.  First, the thread that is passed into the function is found by
    searching through the queues, and the function lingers until the status of this thread is terminated.  Once it is terminated, we know the thread can now continue, 
    so the function can return. The return value, if the function isn't void, is also stored in value_ptr.
    
int worker_mutex_init(worker_mutex_t *mutex, const pthread_mutexattr_t *mutexattr):

    worker_mutex_init initializes the worker_mutex_t structure by initializing the fields appropriately so that the worker threads can then use the worker_mutex_t for synchronization.
    
int worker_mutex_lock(worker_mutex_t *mutex):

    worker_mutex_lock uses the built in __atomic_test_and_set() function to set the lock field of the worker_mutex_t to 1. This function returns the initial value of worker_mutex_t->locked
    and checks this initial value by using a while loop so that the thread will remain in this loop upon context switches until the lock is released. If the thread does not have access to
    the lock, its status is set to blocked and the context is then switched to the scheduler, which will enqueue the thread into the blocked queue instead of the run queue.
    If the thread has access to the lock, the thread will be pointed to by the worker_mutex_t->lockOwner field and then the thread will have access to the subsequent critical section.
    
int worker_mutex_unlock(worker_mutex_t *mutex):

    worker_mutex_unlock will check if the thread calling it is the owner of the lock, and if it is, it will set the worker_mutex_t fields appropriately and it will remove the thread 
    at the head of the blocked queue and enqueue it to the run queue.
    
int worker_mutex_destroy(worker_mutex_t *mutex):

    worker_mutex_destroy will reset the fields of the worker_mutex_t so that it cannot be used until it is re-initialized.
    
static void schedule():

    schedule is the main scheduler function to which the scheduler context is assigned to and to which the thread library is built on.  The first thing done in the schedule function is to 
    disable the timer so the scheduler is not interrupted.  The scheduler then calls the function corresponding to what the selected scheduling policy is, which will store the next thread     to run in curThread.  The status of this thread is changed to "running", the timer is enabled, and the thread is run.  Once the scheduler context takes control again, the thread will      be changed back to a status of ready if it is not blocked or terminated, and it will be added to the appropriate queue based on the scheduling policy.
    If the thread state is blocked, it will be put in the blocked queue, and if it is terminated it will be put in the terminated queue.

static void sched_psjf():

    sched_psjf will search through the run queue to find the thread with the lowest quantums elapsed, which is a field of the TCB structure and it corresponds to how many times the thread
    runs and then is interrupted by the timer to return to the scheduler, which then will increment this field. The QUANTUM macro in the header file determines how long a quantum is, and
    we used 10 to make the timer interrupt every 10ms. The thread with the lowest quantums elapsed is then removed from the run queue and will be run when the scheduler swaps contexts to
    its context.
    
static void sched_mlfq():

    sched_mlfq will search through the 4 levels of queues starting at the top-most queue until it finds the first thread, which corresponds to the thread with a high priority.
    The thread will then be removed from the run queue and the scheduler will swap contexts to its context so it runs next.
    Every time the timer interrupts, the current thread that was running will have its priority lowered, and then upon returning to the scheduler, the thread will then be enqueued
    to a lower level queue depending on its priority. The MLFQ algorithm also requires a global count of quantums elapsed so that the 4 levels of queues can periodically be reset
    according to rule 5 of MLFQ. The S macro in the header determines how many quantums must elapse before the MLFQ is reset. Every time the timer interrupts, the total quantum counter is     incremented and depending on how many quantums have elasped, the MLFQ may be reset (thread prioroties set to 0 and all threads enqueued to the top level queue).

static void create_tcb(worker_t * thread, tcb* control_block, void *(*function)(void*), void * arg):

    The purpose of this function is to modularize the population of a tcb.  A new context is created, the stack for the new tcb is created, and the fields such as thread id, status, and       stack are populated within the tcb. The context in the tcb is then assigned to the given function.

tcb* enqueue(tcb *thread, tcb *queue):

    This is a function to add a new tcb to the given queue.  It either assigns the pointer of the queue to the pointer of the thread if the queue is empty, or traverses to the end of the      queue and appends the thread.

void enqueueMLFQ(tcb* thread):

    This function will place the thread into one of the 4 levels of queues depending on its priority.

tcb* dequeue(tcb *queue):

    Function to dequeue from the given queue.

tcb* dequeuePSJF(tcb *queue):

    This function handles searching for a thread with the lowest quantums elasped and manipulating the run queue appropriately.

void dequeueMLFQ():

    This function handles dequeueing a thread from the highest level queue where a thread is present.

void blockedDequeue():

    This function handles removing the thread at the head of the blocked queue and putting it back in the run queue.

void resetMLFQ():

    This function sets all thread priorities to 0 and then moves them all to the highest level run queue (the main run queue).

static tcb* search(worker_t thread, tcb* queue):

    This function searches for the thread corresponding to the given thread id in the given queue.  It returns the tcb upon a search hit and returns null upon a search miss.

static tcb* searchAllQueues(worker_t thread):

    This function searches all of the queues to find the specified thread so that worker_join can free its data.
    The queues searched depends on the scheduling policy as some of the queues are only used with the MLFQ policy.

int isEmpty(tcb *queue):

    Abstraction to verify if queue is empty

int areQueuesEmpty():

    Checks all of the queues to determine if any threads are present. The queues checked depends on the scheduling policy.

void printQueue(tcb *queue):

    Helper function to print the queue by their thread id's

void toString(tcb *thread):

    Helper function to print information about a thread

static void signal_handler(int signum):

    This function is called upon every timer interrupt. This is the key for scheduling the threads appropriately. It also handles keeping track of the total quantums elasped, resetting
    the MLFQ periodically, and lowering a thread's priority when it completes a time quantum.
 
static void enable_timer():

    Function to enable the timer

static void disable_timer():

    Function to disable timer by setting parameters to 0

void setup_timer():

    Function to set up timer.  Timer is assigned to the signal handler function, the time parameters are set up, and setitimer is called.

int scheduler_benchmark_create_context():

    This function is called upon the very first call of worker_create.  The scheduler and benchmark context are created, the benchmark context is assigned 
    a tcb while the scheduler context is stored in a global variable.  setup_timer is also called, and control is handed over to the scheduler context.

2)
Benchmark Results:

MLFQ:
    external_cal.c:

    5 threads: runtime: 632 microseconds, context switches: 159, turnaround: 436, response: 23
    10 threads: runtime: 585 microseconds, context switches: 186, turnaround: 403.181818, response: 33.272727
    20 threads: runtime: 580 microseconds, context switches: 180, turnaround: 221.380952, response: 25.571429
    50 threads: runtime: 551 microseconds, context switches: 152, turnaround: 136.960784, response: 69.098039
    100 threads: runtime: 544 microseconds, context switches: 262, turnaround: 57.108911, response: 15.673267

    parallel_cal.c:

    5 threads: runtime: 26413 microseconds, context switches: 2199, turnaround: 7847.166667, response: 24.666667
    10 threads: runtime: 25659 microseconds, context switches: 2284, turnaround: 8532.181818, response: 68.818182
    20 threads: runtime: 23512 microseconds, context switches: 1716, turnaround: 6570.476190, response: 144.476190
    50 threads: runtime: 17502 microseconds, context switches: 1618, turnaround: 5358.666667, response: 355.058824
    100 threads: runtime: 16355 microseconds, context switches: 1750, turnaround: 5278.801980, response: 796.693069

    vector_multiply.c:

    5 threads: runtime: 49 microseconds, context switches: 9, turnaround: 21.333333, response: 16.166667
    10 threads: runtime: 50 microseconds, context switches: 14, turnaround: 25.818182, response: 22.727273
    20 threads: runtime: 51 microseconds, context switches: 24, turnaround: 28.428571, response: 26.904762
    50 threads: runtime: 54 microseconds, context switches: 54, turnaround: 30.235294, response: 29.509804
    100 threads: runtime: 52 microseconds, context switches: 104, turnaround: 28.504950, response: 28.148515

PSJF:
    external_cal.c:

    5 threads: runtime: 633 microseconds, context switches: 143, turnaround: 444.833333, response: 24
    10 threads: runtime: 655 microseconds, context switches: 216, turnaround: 469.545455, response: 47
    20 threads: runtime: 595 microseconds, context switches: 190, turnaround: 264.809524, reponse: 59.428571
    50 threads: runtime: 532 microseconds, context switches: 174, turnaround: 109.607843, response: 26.647059
    100 threads: runtime: 609 microseconds, context switches: 274, turnaround: 61.198020, response: 16.574257

    parallel_cal.c:

    5 threads: runtime: 28842 microseconds, context switches: 2269, turnaround: 8418.666667, response: 25
    10 threads: runtime: 26100 microseconds, context switches: 2142, turnaround: 8076.090909 response: 73.818182
    20 threads: runtime: 22584 microseconds, context switches: 1688, turnaround: 6907.476190, response: 143.761905
    50 threads: runtime: 18224 microseconds, context switches: 1638, turnaround: 5667.098039, response: 384.921569
    100 threads: runtime: 15009 microseconds, context switches: 1568, turnaround: 4648.623762, response: 803.287129

    vector_multiply.c:

    5 threads: runtime: 52 microseconds, context switches: 9, turnaround: 21.833333, response: 16
    10 threads: runtime: 50 microseconds, context switches: 14, turnaround: 25.545455, response: 22.454545
    20 threads: runtime: 50 microseconds, context switches: 24, turnaround: 28.238095, response: 26.714286
    50 threads: runtime: 49 microseconds, context switches: 54, turnaround: 27.666667, response: 26.960784
    100 threads: runtime: 51 microseconds, context switches: 104, turnaround: 29.029703, response: 28.673267

3)
Benchmark Result Analysis:

    On average, the MLFQ scheduling policy results in faster run times and increasing the number of threads also improves the run time. However, it appears that sometimes
    increasing the number of threads can actually increse the runtime which could indicate that allocating more data for threads and/or having to switch between more threads 
    can hurt the run time. Additionally, comparing these results to those of the standard pthread library showed that the pthread library results in much faster run times. The run
    times were approximately half that of the run times from using our worker thread library. This is most likely due to the pthread library handling many more cases and implementing
    scheduling algorithms much more elegantly and with more optimizations than our implementation.
