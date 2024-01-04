#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>
#include <ucontext.h>

#define STACK_SIZE SIGSTKSZ

/* Uses a timer and a signal handler to send a signal every second to switch between contexts.  Lags like crazy because of infinite printing so it switches
slower than it normally would.*/

struct itimerval timer;
ucontext_t c1, c2, maincon, current;
int current_context = 1;

void ring(int signum){
    swapcontext(&current, &maincon);
}

void function1() {
    while(1) printf("first function\n");
    while(1);
}

void function2() {
   while(1) printf("second function\n");
   while(1);
}

int main(){
    getcontext(&c1);
	void* stack = malloc(SIGSTKSZ);
	c1.uc_link=NULL;
	c1.uc_stack.ss_sp=stack;
	c1.uc_stack.ss_size=STACK_SIZE;
	c1.uc_stack.ss_flags=0;
	makecontext(&c1, (void *)&function1, 0, NULL);

    getcontext(&c2);
	void* stack2 = malloc(SIGSTKSZ);
	c2.uc_link=NULL;
	c2.uc_stack.ss_sp=stack2;
	c2.uc_stack.ss_size=STACK_SIZE;
	c2.uc_stack.ss_flags=0;
	makecontext(&c2, (void *)&function2, 0, NULL);

	struct sigaction sa;
	memset (&sa, 0, sizeof (sa));
	sa.sa_handler = &ring;
	sigaction (SIGPROF, &sa, NULL);

	timer.it_interval.tv_usec = 0; 
	timer.it_interval.tv_sec = 1;

	timer.it_value.tv_usec = 0;
	timer.it_value.tv_sec = 1;
	setitimer(ITIMER_PROF, &timer, NULL);

	while(1) {
        if(current_context == 1) {
            current_context = 2; 
            current = c2; 
        }
        else {
            current_context = 1; 
            current = c1; 
        }
        swapcontext(&maincon, &current);
    }
	return 0;
}
