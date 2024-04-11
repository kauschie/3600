/*
 
Author: Michael Kausch
Assignment: Lab 11 (b)
Date: 4/11/24
Class: Operating Systems
Instructor: Gordon

 */

/*
 * dotprod.c
 *
 * This program exists in several other directories for cs3600, and the code
 * may be slightly different. It is just a sample program that you may
 * experiment with.
 *
 *     $ gcc -o dotprod dotprod.c -lpthread
 *     $ ./dotprod
 *     80200         sum(i=1 to 400) i = 80200
 *  This threaded program computes the algebraic dot product of two vectors:
 *        a = <2,3,4>  and  b = <-1,3,5>
 *        _   _ 
 *        a . b = (2*-1)+(3*3)+(4*5) = -2+9+20 = 27.
 *
 *  A mutex enforces mutual exclusion on the shared structure: lock before 
 *  updating and unlock after updating.
 *
 *  The main program creates threads which do all the work and then print out 
 *  result upon completion. Before creating the threads, the input data is 
 *  created. The main thread needs to wait for all threads to complete, it 
 *  waits for each one of the threads. We specify a thread attribute value that 
 *  allow the main thread to join with the threads it creates. Note also that 
 *  we free up handles when they are no longer needed.
 *	 
 *  Each thread works on a different set of data. The offset is specified by 
 *  the loop variable 'i'. The size of the data for each thread is indicated
 *  by VECLEN.
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <wait.h>


typedef struct {
	int *a;
	int *b;
	/* int total; */
	int veclen;
} Dotdata;

#define NUM_THRDS 4
#define VEC_LEN 122
Dotdata dotstr;               /* global so all threads can see and use it */
pthread_t callThd[NUM_THRDS];
pthread_mutex_t mutexsum;     /* use a mutex to protect the dot product */
int my_pipes[2];


void *dotprod(void *arg)
{
	/* thread function */
	int i, start, end, len ;
	long offset;
	int localsum, *x, *y;
	offset = (long)arg;

	len = dotstr.veclen;
	start = offset * len;
	end   = start + len;
	x = dotstr.a;
	y = dotstr.b;

	localsum = 0;
	for (i=start; i<end; i++) {
		localsum += (x[i] * y[i]);
	}

	pthread_mutex_lock (&mutexsum);
	write(my_pipes[1], &localsum, sizeof(localsum));
	/* dotstr.total += localsum; */
	pthread_mutex_unlock (&mutexsum);
	pthread_exit((void *)0);
	// return (void*)0;
}


int main (int argc, char *argv[])
{
	long i;
	int *a, *b;
	void *status;
	int cpid = 0;
	
	int wStatus = 0;
	/*pthread_attr_t attr;*/

	/* Assign storage and initialize values in the vectors */
	a = (int *)malloc(NUM_THRDS * VEC_LEN * sizeof(int));
	b = (int *)malloc(NUM_THRDS * VEC_LEN * sizeof(int));
	for (i=0; i<VEC_LEN * NUM_THRDS; i++) {
		a[i] = 1.0;
		b[i] = a[i];  /* over written in the next statement */
		b[i] = (i+1); /* integers from 1 to VECLEN*NUMTHRDS*/
	}

	dotstr.veclen = VEC_LEN; 
	dotstr.a = a; 
	dotstr.b = b; 
	// dotstr.total = 0;

	pthread_mutex_init(&mutexsum, NULL);

	pipe(my_pipes);

	cpid = fork();
	if (cpid == 0) {
		// child
		int	child_accumulator = 0;
		int val = 0;

		close(my_pipes[1]);

		while (read(my_pipes[0], &val, sizeof(val)) > 0)
			child_accumulator += val;

		// display sum

		printf("Child process sum from pipe: %d\n", child_accumulator);
		fflush(stdout);

		/* now you can safely print out the results and cleanup */
		/* printf ("the dot product sum: %d\n", dotstr.total); */
		if (a)
			free(a);
		if (b)
			free(b);
		
		close(my_pipes[0]);
		exit(0xDEADC0DE);
	} else {
		// parent 
		close(my_pipes[0]);
		char buf[100] = {0};

		/* create NUMTHRDS threads */
		for (i=0; i<NUM_THRDS; i++) {
			pthread_create(&callThd[i], NULL, dotprod, (void *)i);
		}
		
		/* wait on all the threads to finish */
		for (i=0; i<NUM_THRDS; i++) {
			pthread_join(callThd[i], &status);
		}
		// printf("finished joining threads"); fflush(stdout);

		pthread_mutex_destroy(&mutexsum);

		close(my_pipes[1]);

		pid_t child_pid = wait(&wStatus);
		sprintf(buf, "Child (%d) exit code: %d\n", child_pid, WEXITSTATUS(wStatus)); 
        printf("%s",buf);
        exit(0);
	}
	
}

