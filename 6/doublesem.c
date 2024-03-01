
/*
Name: Michael Kausch
Class: Operating Systems
Professor: Gordon
Date: 2/28/23

Assignment: mythread.c
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <pthread.h> // for threads
#include <semaphore.h>

// with threads, global vars are shared between them
sem_t sem[2];
int a;

void * hello(void* arg)
{
    int tnum = (int)(long)arg;
    int my_sem_num = tnum-1;
    int other_sem_num = !(my_sem_num);
    int count = 0;

    while (count < 8) {
        // grab the semaphore
        sem_wait(&sem[my_sem_num]); // stops here until the value is greater than 0
                        // one will see it at one and get in, the other will see its decremented value

        // critical section - more than one process/thread is running the
        // code at the same time and accessing a shared resource
        //      -- need to protect so only one is running at a time
        a = tnum;
        printf("hello from %i\n", tnum);
        printf("%i   a:%i\n", tnum, a);
        // critical section end
        // release the semaphore
        sem_post(&sem[other_sem_num]);

        ++count;
    }
    return (void*)0;
}

int main(void)
{
    void* status;
    sem_init(&sem[0], 0, 1); // want to make sure that one of the threads can grab
    sem_init(&sem[1], 0, 0); // want to make sure that one of the threads can grab
                          // when it does, it will decrement the value

    pthread_t tid[2];
    pthread_create(&tid[0], NULL, hello, (void*)1);
    pthread_create(&tid[1], NULL, hello, (void*)2);
    pthread_join(tid[0], &status); // wait here until the the thread finishes and
    pthread_join(tid[1], &status);//    join it with the main thread

    sem_destroy(&sem[0]);
    sem_destroy(&sem[1]);

    return 0;
}
