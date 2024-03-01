/*
 * Name: Michael Kausch
 * Assignment: Lab6
 * Class: Operating Systems
 * Professor: Gordon
 * Date: 2/29/2024
 *
 *  echo.c
 *
 *  Demonstrate classic producer/consumer problem; the parent thread (the 
 *  producer) writes one character at a time from a file into a shared buffer
 *
 *  The buffer is shared because it is defined globally.
 *
 *  The consumer reads from buffer and writes it to a log file. With no 
 *  synchronization, the consumer will read the same character multiple times
 *  because the producer is slower than the consumer.
 *  Output is not deterministic - it varies across executions. 
 *
 *  This is a problem of concurrency. When two threads access shared resources 
 *  without synchronization it is a race condition as to which thread gets 
 *  there first or next. Without synchronization, the consumer may read a value
 *  multiple times (empty buffer) or the producer overwrite a value before the 
 *  consumer had a chance to read it. 
 *
 *    $ gcc -o echo echo.c -lpthread   # link in POSIX pthread library  
 *    $ ./echo 
 */

#include <pthread.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/types.h>

#define DEBUG 0 
#define BUFSIZE 1

union {
	int val;
	struct semid_ds *buf;
	unsigned short *array;
	struct seminfo *__buf;
} my_semun;

struct asynchInfo {
    int semid;
} asI;

/* thread function prototypes */
void * consumer(void *arg); 
void * producer(void *arg);
void init_sem(void);

/* A note on the differences between forks and threads. Variables local to 
 * main that exist before the fork are inherited by the child but not shared.
 * Threads, since they are functions, can only see globals. These globals are
 * not only visible but shared by all threads since threads share user space.
 */

int retval;
int LIMIT = 50;  /* for testing read 55 chars from the file */
char sharedBuf[1];           /* 1 char buffer */
int fib(int);
int fd[2];
int semid;
FILE * fin;
FILE * fout;
int doneReading = 0;

struct sembuf grab_R[2], release_R[1];
struct sembuf grab_W[2], release_W[1];

int main(int argc, char * argv[])
{
    void * status;

    // fin = fopen("poem", "r");
    fout = fopen("log", "w");

    if (!fout) {
        printf("Error opening log");
    }   

    fd[0] = open("poem", O_RDONLY, 0644);
    // fd[1] = open("log", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    pthread_t tid[2];

    if (fd[0] < 0)
        return 1;
    // if (fd[1] < 0)
    //     return 1;

    /* check if n was given on command-line */
    if (argc > 2){
        printf("Usage: %s [limit]\n", argv[0]);
    } else if (argc == 1) {
        LIMIT = 50;
    } else {
        LIMIT = atoi(argv[1]);
    }
    
    if ((semid = semget(IPC_PRIVATE, 2, 0666 | IPC_CREAT)) == -1) {
        printf("Error"); 
        return 1;
    }

    init_sem();

    pthread_create(&tid[0], NULL, producer, (void*)&asI);
    pthread_create(&tid[1], NULL, consumer, (void*)&asI);

    pthread_join(tid[0], &status);
    pthread_join(tid[1], &status);
    close(fd[0]);
    fclose(fout);
    semctl(semid, 0, IPC_RMID); /* remove semaphore from ipcs */

    return 0;
}

void * producer(void *arg)
{
    int numRead;
    int count = 0;

    while ( !doneReading) {
        semop(semid, grab_R, 2);
        if (((numRead = read(fd[0], sharedBuf, 1)) > 0)
            && count < LIMIT) {
            // printf("my turn to read...\n");kk
            // printf("num read is %d\n", numRead);
            // printf("string is %s\n", sharedBuf);
            count++;
            semop(semid, release_W, 1);
        } else {
            doneReading = 1;
            semop(semid, release_W, 1);
        }
    }
    // critical section

    return (void*)0;

}

void * consumer(void *arg)
{
    doneReading = 0;

    pid_t tid = syscall(SYS_gettid);
	fprintf(fout, "consumer thread pid: %d tid: %d \n", getpid(), tid);
    
    do {
        semop(semid, grab_W, 2);

        if (!doneReading) {
            // printf("my turn to write...\n");
            // write(fd[1], sharedBuf, strlen(sharedBuf));
            // fsync(fd[1]);
            fputc(sharedBuf[0], fout);
            // printf("%s", sharedBuf);
            // fflush(stdout);
            fflush(fout);
            //memset(sharedBuf, 0, 1);

        } else
            break;
        semop(semid, release_R, 1);
    } while (!doneReading);
    
    return (void*)0;


}

void init_sem(void)
{

    grab_R[0].sem_num = 0;
    grab_R[1].sem_num = 0;
    grab_R[0].sem_flg = SEM_UNDO;
    grab_R[1].sem_flg = SEM_UNDO;
    grab_R[0].sem_op = 0;
    grab_R[1].sem_op = 1;
    release_R[0].sem_num = 0;
    release_R[0].sem_flg = SEM_UNDO;
    release_R[0].sem_op = -1;

    grab_W[0].sem_num = 1;
    grab_W[1].sem_num = 1;
    grab_W[0].sem_flg = SEM_UNDO;
    grab_W[1].sem_flg = SEM_UNDO;
    grab_W[0].sem_op = 0;
    grab_W[1].sem_op = 1;
    release_W[0].sem_num = 1;
    release_W[0].sem_flg = SEM_UNDO;
    release_W[0].sem_op = -1;

    my_semun.val = 0;   // allow read first op
    semctl(semid, 0, SETVAL, my_semun);
    my_semun.val = 1; // write second op
    semctl(semid, 1, SETVAL, my_semun);
    
}

int fib(int n)
{
	if (n == 1 || n == 2)
		return 1;
	return fib(n-1) + fib(n-2);
}


