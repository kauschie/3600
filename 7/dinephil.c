/*
Name: Michael Kausch
Class: Operating Systems
Professor: Gordon
Date: 3/6/24

Assignment: dinephil.c
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
sem_t forks[5];
pthread_mutex_t busboy;

int n_eating = 4;
int eating[5] = {0, 0, 0, 0, 0};
int done = 0;

void * philosopher(void* arg)
{
    int tnum = (int)(long)arg;
    int my_forks_numbers[2] = {tnum, (tnum + 1) % 5};

    if (my_forks_numbers[0] > my_forks_numbers[1]) {
        // swap
        int tmp = my_forks_numbers[0];
        my_forks_numbers[0] = my_forks_numbers[1];
        my_forks_numbers[1] = tmp;
    }

    //int count = 0;


    while (!done) {
        // grab 2 forks 
        pthread_mutex_lock(&busboy);

        sem_wait(&forks[my_forks_numbers[0]]);
        sem_wait(&forks[my_forks_numbers[1]]);
        pthread_mutex_unlock(&busboy);
        
        // critical section
        // eating
        ++eating[tnum];
        printf("%i eating %i %i %i %i %i\n", tnum, eating[0], eating[1],
                                    eating[2], eating[3], eating[4]);
        // end critical section
        //
        sem_post(&forks[my_forks_numbers[0]]);
        sem_post(&forks[my_forks_numbers[1]]);
        if (eating[tnum] >= n_eating)
            done = 1;
    }
    return (void*)0;
}

int main(int argc, char * argv[])
{
    void* status;
    int i;

    if (argc > 1)
        n_eating = atoi(argv[1]);

    pthread_mutex_init(&busboy, NULL); // unlock the mutex
    
    for (i = 0; i < 5; i++) {
        sem_init(&forks[i], 0, 1); // want to make sure that one of the threads can grab
    }
    
   // sem_init(&sem[1], 0, 0); // want to make sure that one of the threads can grab
                          // when it does, it will decrement the value
    pthread_t tid[5];
    for (i = 0; i < 5; i++) {
        pthread_create(&tid[i], NULL, philosopher, (void*)(long)i);
    }

    for (i = 0; i < 5; i++) {
        pthread_join(tid[i], &status); // wait here until the the thread finishes and
    }

    for (i = 0; i < 5; i++) {
        sem_destroy(&forks[i]);
    }


    return 0;
}
