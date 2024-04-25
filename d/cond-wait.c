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

/* create a static mutex and a static condition variable - safest method */
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;  
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

#define DEBUG 1 

/* For safe condition variable usage, must use a boolean predicate and  
 * a mutex with the condition.                  
 */
int work2do = 0;
int loopcnt = 10;

void *profunc(void *); 

int main(int argc, char **argv)
{
	if (argc==2) 
		loopcnt = atoi(argv[1]);
	int rc=0;
	int i;
	pthread_t tid;
	time_t T;

	time(&T);
	printf("start time: %s", ctime(&T));

	if (DEBUG)
		printf("Create producer thread\n");
	rc = pthread_create(&tid, NULL, profunc, NULL);
	if (rc != 0)
		perror("pthread_create()");

	/*----------*
	 * consumer *
	 *----------*/

	for (i=1; i<=loopcnt; i++) {

		if ((rc=pthread_mutex_lock(&mutex)!=0))
			perror("pthread_mutex_lock()");

		/* this protects against spurious wakeups */
		while (work2do==0) {
			if ((rc=pthread_cond_wait(&cond, &mutex))!= 0)
				perror("cond_wait()");
		}
		if (DEBUG)
			printf("consumer critical code %d\n",i);
		work2do--;

		/* give the producer the aok */
		if ((rc=pthread_cond_signal(&cond)!=0))
			perror("pthread_cond_signal");
		if ((rc=pthread_mutex_unlock(&mutex)!=0))
			perror("pthread_mutex_unlock()");
	}

	pthread_join(tid, NULL);

	printf("Main completed - thread joined.\n");
	printf("Work2do: %d.\n",work2do);
	time(&T);
	printf("end time: %s", ctime(&T));

	exit(0);
}

/* PRODUCER THREAD FUNCTION */
void *profunc(void *dummy)
{
	int rc, i;

	for (i=1; i<=loopcnt; i++) {

		if ((rc = pthread_mutex_lock(&mutex)!=0))
			perror("mutex_lock()");

		/* this protects against spurious wakeups */
		while (work2do==1) {
			if ((rc=pthread_cond_wait(&cond, &mutex))!= 0)
				perror("cond_wait()");
		}
		/* critical code */
		if (DEBUG)
			printf("producer critical code %d\n",i);
		work2do++;

		if ((rc=pthread_cond_signal(&cond)!=0))
			perror("pthread_cond_signal");
		if ((rc = pthread_mutex_unlock(&mutex)!=0))
			perror("mutex_unlock()");
	}  
	return NULL;
}

