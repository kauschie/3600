/*
Name: Michael Kausch
Class: Operating Systems
Professor: Gordon
Date: 3/7/24

Assignment: rvlab7.c
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
//#include <semaphore.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <sys/fcntl.h>
// #define BUFSIZE 256

// with threads, global vars are shared between them
//sem_t forks[5];
pthread_mutex_t busboy;
int n_eating = 4;
int delay = 0;
int eating[5] = {0, 0, 0, 0, 0};
int done = 0;
int semid = 0;
struct sembuf grab_f[5][2], release_f[5];
int fib(int n);
void init_sem(void);
// int fibs[BUFSIZE];



union {
	int val;
	struct semid_ds *buf;
	unsigned short *array;
	struct seminfo *__buf;
} my_semun;


void * philosopher(void* arg)
{
    int tnum = (int)(long)arg;
    int my_forks[2] = {tnum, (tnum + 1) % 5};

    if (my_forks[0] > my_forks[1]) {
        // swap
        int tmp = my_forks[0];
        my_forks[0] = my_forks[1];
        my_forks[1] = tmp;
    }

    //int count = 0;


    while (!done) {
        fib(delay);
        // grab 2 forks 
        pthread_mutex_lock(&busboy);
        // if (done) break;
        semop(semid, &(grab_f[my_forks[0]][0]), 2);
        // semop(semid, &(grab_f[my_forks[0]]), 1);

        semop(semid, &(grab_f[my_forks[1]][0]), 2);
        // semop(semid, &(grab_f[my_forks[1]][1]), 1);

        pthread_mutex_unlock(&busboy);
        if (done) {
            semop(semid, &release_f[my_forks[0]], 1);
            semop(semid, &release_f[my_forks[1]], 1);
            break;
        } 

        // critical section
        // eating
        ++eating[tnum];

        printf("%i eating %i %i %i %i %i\n", tnum, eating[0], eating[1],
                                    eating[2], eating[3], eating[4]);
        fflush(stdout);
        // end critical section
        
        semop(semid, &release_f[my_forks[0]], 1);
        semop(semid, &release_f[my_forks[1]], 1);
        // sem_post(&forks[my_forks[0]]);
        // sem_post(&forks[my_forks[1]]);
        if (eating[tnum] >= n_eating)
            done = 1;
    }
    return (void*)0;
}

int main(int argc, char * argv[])
{
    void* status;
    int i;

    if (argc == 3) {
        delay = atoi(argv[1]);
        n_eating = atoi(argv[2]);
    } else {
        printf("Usage: %s <delay> <max eats>\n", argv[0]);
        delay = 40;
        n_eating = 10;
        // return 1;
    }

    pthread_mutex_init(&busboy, NULL); // unlock the mutex
    
    if ((semid = semget(IPC_PRIVATE, 5, 0666 | IPC_CREAT)) == -1) {
        printf("Error"); 
        return 1;
    }

    init_sem();
    
   // sem_init(&sem[1], 0, 0); // want to make sure that one of the threads can grab
                          // when it does, it will decrement the value
    pthread_t tid[5];
    for (i = 0; i < 5; i++) {
        pthread_create(&tid[i], NULL, philosopher, (void*)(long)i);
    }

    for (i = 0; i < 5; i++) {
        pthread_join(tid[i], &status); // wait here until the the thread finishes and
    }

    // for (i = 0; i < 5; i++) {
    //     sem_destroy(&forks[i]);
    // }

    semctl(semid, 0, IPC_RMID); /* remove semaphore from ipcs */

    return 0;
}

void init_sem(void)
{
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 2; j++) {
            grab_f[i][j].sem_num = i;
            grab_f[i][j].sem_flg = SEM_UNDO;
            grab_f[i][j].sem_op = j;
        }
        // grab_f[i][0].sem_num = i;
        // grab_f[i][1].sem_num = i;
        // grab_f[i][0].sem_flg = SEM_UNDO;
        // grab_f[i][1].sem_flg = SEM_UNDO;
        // grab_f[i][0].sem_op = 0;
        // grab_f[i][1].sem_op = 1;
        release_f[i].sem_num = i;
        release_f[i].sem_flg = SEM_UNDO;
        release_f[i].sem_op = -1;
        my_semun.val = 0;   // allow read first op
        semctl(semid, i, SETVAL, my_semun);
    }
}

int fib(int n)
{
    if (n == 1 || n == 2)
        return 1;
    return fib(n-1) + fib(n-2);
}

// int fib(int n)
// {
//     /* We will write this function in class together. */
//     if (n == 1 || n == 2) {
//         if (fibs[n-1] == 0) {
//             fibs[n-1] = 1;
//         }   

//         return fibs[n-1];
//     }

//     int fib1 = fibs[n-2]; // fib(n-1)
//     if (fib1 == 0) {
//         fibs[n-2] = fib(n-1);
//         fib1 = fibs[n-2];
//     }

//     int fib2 = fibs[n-3]; // fib(n-2)
//     if (fib2 == 0) {
//         fibs[n-3] = fib(n-2);
//         fib2 = fibs[n-3];

//     }

//     return fib1 + fib2;
// }