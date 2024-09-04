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

#define DEBUG 0 
#define NUM_THREADS 5

/* For safe condition variable usage, must use a boolean predicate and  
 * a mutex with the condition.                  
 */
// int loopcnt = 10;

int work2do[NUM_THREADS];
pthread_cond_t cond[NUM_THREADS];
pthread_mutex_t mutex[NUM_THREADS];
int fibnumber = 1;

void *workerThread(void * thread_num); 

int main(int argc, char **argv)
{
	if (argc==2) 
		fibnum = atoi(argv[1]);
	
	int rc=0;
	int i;
	pthread_t tids[NUM_THREADS];

	if (DEBUG)
		printf("Create producer threads\n");
	

	// init vars
	for (int i = 0; i < NUM_THREADS; i++) {
		// init its condition variable
		if (pthread_cond_init(&cond[i], NULL) != 0) {
		perror("pthread_cond_init() error");
		exit(2);
		}
		
		// lock it initially
		if (pthread_mutex_lock(&mutex[i]) != 0) {
			perror("pthread_mutex_lock error");
			exit(3);
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
	printf("parent writes time: %s", myNum, ctime(&T));
	
	
	if ((rc=pthread_mutex_lock(&mutex[0])!=0))
		perror("pthread_mutex_lock_neighbor");

	//signal neighbor
	if ((rc=pthread_cond_signal(&cond[myNum+1])!=0))
		perror("pthread_cond_signal");

	work2do[0]++;

	// unlock neighbor's mutex
	if ((rc = pthread_mutex_unlock(&mutex[myNum+1])!=0))
		perror("mutex_unlock_neighbor()");

	for (int i = 0; i < NUM_THREADS; i++) {
		pthread_join(tid[i], NULL);
	}

	exit(0);
}


/* THREAD FUNCTION */
void *workerThread(void *thread_num)
{
	int myNum = (int)(long)thread_num;

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
		printf("producer critical code %d\n",i);
	time(&T);
	printf("thread %d writes time: %s", myNum, ctime(&T));

	pthread_mutex_unlock(&mutex[myNum]);

	if(myNum < NUM_THREADS-1) {
		// lock neighbors mutex
		if ((rc=pthread_mutex_lock(&mutex[myNum+1])!=0))
			perror("pthread_mutex_lock_neighbor");


		//signal neighbor
		if ((rc=pthread_cond_signal(&cond[myNum+1])!=0))
			perror("pthread_cond_signal");

		// inc neighbor's flag
		work2do[myNum+1]++;

		// unlock neighbor's mutex
		if ((rc = pthread_mutex_unlock(&mutex[myNum+1])!=0))
			perror("mutex_unlock_neighbor()");


	}

	return NULL;
}

int fib(int n)
{
	if (n == 1 || n == 2)
		return 1;
	return fib(n-1) + fib(n-2);
}