/* timed-wait.c 
 * a condition timed wait behaves like a sleep call
 */

#define _OPEN_THREADS
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>

int main(void) {
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	time_t T;
	struct timespec t;

	if (pthread_mutex_init(&mutex, NULL) != 0) {
		perror("pthread_mutex_init() error");
		exit(1);
	}

	if (pthread_cond_init(&cond, NULL) != 0) {
		perror("pthread_cond_init() error");
		exit(2);
	}

	if (pthread_mutex_lock(&mutex) != 0) {
		perror("pthread_mutex_lock error");
		exit(3);
	}

	time(&T);
	t.tv_sec = T + 5; /* set timedwait to 5 secs + absolute time */ 
	printf("starting timed wait at %s", ctime(&T));
	if (pthread_cond_timedwait(&cond, &mutex, &t) != 0) {
		if (errno == EAGAIN)
			puts("wait timed out");
		else {
			perror("pthread_cond_timedwait error");
			exit(4);
		}
	}
	time(&T);
	printf("timedwait over at %s", ctime(&T));
	return 0;
}

