/*
 * cond-wait.c
 * demonstrate alternating P/C synchronization with one mutex and one 
 * condition variable; producer thread starts first
 *
 * $valgrind --tool=helgrind ./cond-wait 2>out
 * $strace -f ./cond-wait 2>out
 *
 * ./cond-wait 10000
 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>

/* create a static mutex and a static condition variable - safest method */

#ifndef DEBUG
#define DEBUG 0
#endif

#ifndef NUM_THREADS
#define NUM_THREADS 4
#endif

/* For safe condition variable usage, must use a boolean predicate and  
 * a mutex with the condition.                  
 */
// int loopcnt = 10;

int work2do[NUM_THREADS];
pthread_cond_t cond[NUM_THREADS];
pthread_mutex_t mutex[NUM_THREADS];
int fibnumber = 1;

void *workerThread(void * thread_num);
int fib(int n);

int main(int argc, char **argv)
{
	if (argc==2) {
		fibnumber = atoi(argv[1]);
	} else {
		printf("Usage: %s <delay> <max eats>\n", argv[0]);
		if (argc > 2) {
			perror("bad program usage");
			exit(0xDEADC0DE);
		}
	}
	
	int rc=0;
	// int i;
	pthread_t tids[NUM_THREADS];

	if (DEBUG)
		printf("Create producer threads\n");
	

	// init vars
	for (int i = NUM_THREADS-1; i >= 0; i--) {
		// init its condition variable
		if (pthread_cond_init(&cond[i], NULL) != 0) {
		perror("pthread_cond_init() error");
		exit(2);
		}
		
		// lock it initially
		if (pthread_mutex_init(&mutex[i], NULL) != 0) {
		perror("pthread_mutex_init() error");
		exit(1);
		}

		work2do[i] = 0;

		// create the thread
		rc = pthread_create(&tids[i], NULL, workerThread, (void*)(long)i);
		if (rc != 0)
			perror("pthread_create()");
		else {
			printf("Thread %d created.\n",i);
		}


	}

	time_t T;
	time(&T);
	printf("parent writes time: %s", ctime(&T));
	
	// start first thread
	if ((rc=pthread_mutex_lock(&mutex[0])!=0))
		perror("pthread_mutex_lock_neighbor");

	work2do[0]++;

	//signal neighbor
	if ((rc=pthread_cond_signal(&cond[0])!=0))
		perror("pthread_cond_signal");

	
	// unlock neighbor's mutex
	if ((rc = pthread_mutex_unlock(&mutex[0])!=0))
		perror("mutex_unlock_neighbor()");


	// join all threads before finishing
	for (int i = 0; i < NUM_THREADS; i++) {
		pthread_join(tids[i], NULL);
	}

	exit(0);
}


/* THREAD FUNCTION */
void *workerThread(void *thread_num)
{
	int myNum = (int)(long)thread_num;
	int rc = 0;

	time_t T;

	if (DEBUG) {
		printf("thread %d starting\n", myNum);
	}

	// lock my mutex
		if ((rc = pthread_mutex_lock(&mutex[myNum])!=0))
			perror("mutex_lock_self()");

	// while (!myflag)
	while (work2do[myNum]==0) {
		// wait on condition
		if ((rc=pthread_cond_wait(&cond[myNum], &mutex[myNum]))!= 0)
			perror("cond_wait()");
	}

	/* critical code */
	if (DEBUG)
		printf("thread critical code %d\n",myNum);
	time(&T);
	printf("thread %d writes time: %s", myNum, ctime(&T));
	
	if (DEBUG && fibnumber > 1) {
		printf("fib used and calculated to be %d\n", fib(fibnumber));
	} else {
		fib(fibnumber);
	}

	pthread_mutex_unlock(&mutex[myNum]);

	if(myNum < NUM_THREADS-1) {
		// lock neighbors mutex
		if ((rc=pthread_mutex_lock(&mutex[myNum+1])!=0))
			perror("pthread_mutex_lock_neighbor");

		// inc neighbor's flag
		work2do[myNum+1]++;

		//signal neighbor
		if ((rc=pthread_cond_signal(&cond[myNum+1])!=0))
			perror("pthread_cond_signal");


		// unlock neighbor's mutex
		if ((rc = pthread_mutex_unlock(&mutex[myNum+1])!=0))
			perror("mutex_unlock_neighbor()");


	}

	return NULL;
}

int fib(int n)
{
	// if(DEBUG)
		// printf("fib called with arg %d\n",n);
	if (n == 1 || n == 2)
		return 1;
	return fib(n-1) + fib(n-2);
}