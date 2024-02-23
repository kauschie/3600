/*
 * __lab5.c
 *
 * You may start with this program for lab-5
 * 
 * This file uses shared memory. If you do not understand how shared memory 
 * works, review week-4 examples. Your job in lab-5 is to add a semaphore 
 * to control order. You know things have gone awry when you run the code:
 *  
 *    $ make lab5
 *    $ ./lab5
 *    child reads: 0
 *
 * The desired result is for the parent to compute fib(n), write the result to
 * shared memory then the child reads the result and displays it to the screen.
 * The problem is that things are out of order - by the time the parent computes
 * fib(10) the child has already read memory; i.e., the parent modifies the 
 * segment too late.
 * 
 * This scenario is a race condition. For example, if you pass a small enough 
 * number to fib, the child generally grabs the value OK:
 *
 *    $ ./lab5 10
 *    child reads: 55 
 *
 * but this may not work
 *
 *    $ ./lab5 18
 *    child reads: 0  <---- wrong results 
 *
 * To fix this problem you need to add a semaphore to control order. You want 
 * the parent to grab the semaphore first. Meanwhile the child is blocked on 
 * the semaphore. After the parent writes fib(n) to memory the parent releases 
 * the semaphore and the child can then grab it.
 * 
 */

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

#define BUFSIZE 256
int status;
int fib(int);
int fibs[BUFSIZE];
int fib1(int n);

union {
	int val;
	struct semid_ds *buf;
	unsigned short *array;
	struct seminfo *__buf;
} my_semun;

struct sembuf grab[2], release[1];

int main(int argc, char **argv)
{
    int n;
    char buf[BUFSIZE];
    pid_t cpid;
    int shmid, semid; 
    int *shared;
    memset(fibs, 0, sizeof(int)*BUFSIZE);

    /* check if n was given on command-line */
    if (argc >= 2){
        n = atoi(argv[1]);
        if (n < 1){
            printf("Usage: %s n (where n is a positive integer)\n", argv[0]);
            printf("Number <= 0 not allowed, setting to 25\n");
            n = 25;
        }
    }
    else
        n = 25;

    /* IPC_PRIVATE will provide a unique key without using an ipckey 
     * it works with related processes but not unrelated ones - it is
     * a safe way to get a ipckey to use in a fork */
    shmid = shmget(IPC_PRIVATE, sizeof(int)*100, IPC_CREAT | 0666);
    /* attach and initialize memory segment */
    shared = shmat(shmid, (void *) 0, 0);
    *shared = 0;

    /* Just above we created some shared memory.  */
    /* Enough to hold 100 4-byte integers.        */

    // semaphore setup
    if ((semid = semget(IPC_PRIVATE, 1, 0666 | IPC_CREAT)) == -1) {
        printf("Error - %s\n", strerror(errno));
        _exit(1);
    }

    grab[0].sem_num = 0;
    grab[1].sem_num = 0;
    grab[0].sem_flg = SEM_UNDO;
    grab[1].sem_flg = SEM_UNDO;
    grab[0].sem_op = 0;
    grab[1].sem_op = 1;
    release[0].sem_num = 0;
    release[0].sem_flg = SEM_UNDO;
    release[0].sem_op = -1;
    my_semun.val = 1;
    semctl(semid, 0, SETVAL, my_semun);

    cpid = fork();

    if (cpid < 0) {
        printf("Error - fork command\n");
        fflush(stdout);
        exit(0);
    }

    if (cpid == 0) {
        /* CHILD */
        /* Attach to shared memory -
         * both child and parent must do this
         * but the parent can do it before the fork */

        /* child reads and displays shared memory */

        /* critical section start*/
        int fd = open("log", O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd < 0) {
            perror("open");
            exit(EXIT_FAILURE);
        }

        semop(semid, grab, 2);
        int val = *shared;
        //semop(semid, release, 1);

        /* critical section end */

        sprintf(buf, "child reads: %d\n", val);
        write(1, buf, strlen(buf));

        // write it to a file
        write(fd, buf, strlen(buf));
        close(fd);

        shmdt(shared); /* detach from segment */
        exit(0);
    } else {
        /* PARENT */
        /* Parent computes fib(n) and writes it to shared memory */

        /* critical section */
        *shared = fib1(n);
        semop(semid, release, 1);
        /* end critical section */

        wait(&status); 

        shmdt(shared);              /* detach from segment   */
        shmctl(shmid, IPC_RMID, 0); /* remove shared segment */
        semctl(semid, 0, IPC_RMID); /* remove semaphore from ipcs */
    }
        

    return 0;
}

/* Some busy work for the parent */
// fib with memoization
int fib(int n)
{
    /* We will write this function in class together. */
    if (n == 1 || n == 2) {
        if (fibs[n-1] == 0) {
            fibs[n-1] = 1;
        }   

        return fibs[n-1];
    }

    int fib1 = fibs[n-2]; // fib(n-1)
    if (fib1 == 0) {
        fibs[n-2] = fib(n-1);
        fib1 = fibs[n-2];
    }

    int fib2 = fibs[n-3]; // fib(n-2)
    if (fib2 == 0) {
        fibs[n-3] = fib(n-2);
        fib2 = fibs[n-3];

    }

    return fib1 + fib2;
}

int fib1(int n)
{
    if (n == 1 || n == 2)
        return 1;
    else
        return fib1(n-1) + fib1(n-2);

}



