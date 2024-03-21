/*
Name: Michael Kausch
Class: Operating Systems
Professor: Gordon
Date: 3/14/24

Assignment: rvlab8.c

 *  rvlab8.c    CMPS 3600 LAB-8 
 *  5 threads compete for a shared resource 
 *  modify to add fork/execve calls
 * 
 *  After re-joining all threads in main either do a fork followed by an exec 
 *  or an exec without a fork to cleanup semaphores 
 *
 * Usage examples:
 *    ./lab8    means show a Usage statement and end progam
 *    ./lab8 0  means call execve() from parent without a fork
 *    ./lab8 1  means calls execve() from a child process
 *
 */

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/wait.h>

#define BATON 0
#define DEBUG 0
#define BUFSZ 100

union {
	int val;
	struct semid_ds *buf;
	unsigned short *array;
	struct seminfo *__buf;
} my_semun;

union semun {
    int val;                 /* value for SETVAL */
    struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
    unsigned short  *array;  /* Array for GETALL, SETALL */
    struct seminfo  *__buf;  /* Buffer for IPC_INFO (Linux-specific) */
} my_semun;

const char *nameList[] = {"rv1","rv2","rv3","rv4","rv5"};

typedef struct thrData {
    const char *name;
    pthread_t thread;
    int num;
    int rc;   /* holds the return code from pthread_create */
} Runner;

/* each thread has its own baton grab and release operation */
struct sembuf baton_grab[5][1];
struct sembuf baton_release[5][1];
void cleanup();
void zeroOutBuf(char *str);

int MAX_TURNS = 1;  /* all 5 threads have MAX_TURNS turns at grabbing baton -
                     * in first round sequential order is imposed - after 
                     * that grabbing would be a free-for-all */

static int semid = 0, status;
int nsems = 1;

void *thrfunc(void *p);

int main(int argc, char* argv[], char* envp[])
{
    int i, cpid;

    char fork_flag = 0;  /* 0 means exec without fork; 1 means fork then exec */

    if (argc < 2) {
        printf("Usage: ./xlab8 flg (0 means no fork and 1 means fork)\n");
        printf("Examples:\n");
        printf("\t%s   \tmeans show this Usage statement\n", argv[0]);
        printf("\t%s 0 \tmeans call execve without fork\n", argv[0]);
        printf("\t%s 1 \tmeans fork a child that calls execve\n", argv[0]);
        //You may show the Usage examples here.
        exit(0);
    }

    fork_flag = atoi(argv[1]);

    char pathname[200];
    getcwd(pathname,200);
    strcat(pathname,"/foo"); /* foo file must exist */
    key_t ipckey = ftok(pathname, 5);
    if (ipckey < 0) {
        printf("Error creating ipckey! Check for foo file.\n");
        exit(-1);
    }

    /* setup semaphore operation */
    for (i=0; i<5; i++) {
        baton_grab[i][0].sem_num = BATON;
        baton_grab[i][0].sem_op = -(i+1);
        baton_grab[i][0].sem_flg = SEM_UNDO;

        baton_release[i][0].sem_num = BATON;
        baton_release[i][0].sem_op = (i==4) ? 1 : +(i+2);
        baton_release[i][0].sem_flg = SEM_UNDO;
    }

    /* create one semaphore */
    if ((semid = semget(ipckey, nsems, 0666 | IPC_CREAT)) < 0) {
        perror("Error creating semaphores ");
        exit(EXIT_FAILURE);
    } else {
        if (DEBUG)
            printf("Semid: %d \n", semid);
    }

    /* initialize value of semaphore to 1 to let only first thread in */  
    my_semun.val = 1;
    semctl(semid, BATON, SETVAL, my_semun);

    /* create array of structures to be passed to each thread */
    Runner thrds[5];
    Runner *thr;

    /* create threads in reverse order to demonstrate semaphore is working */
    for (i=4; i>=0; i--) {
        thr = &thrds[i];
        thr->name = nameList[i];
        thr->num = i;
        thr->rc = pthread_create( &thr->thread, NULL, thrfunc, thr);
        printf("created %d\n", i);
        fflush(stdout);
    }

    /* rejoin all threads */
    for (i=0; i<5; i++) {
        thr = &thrds[i];
        if (!thr->rc && pthread_join(thr->thread, NULL)) {
            printf("error joining thread for %s", thr->name);
            exit(1);
        } else {
            printf("\njoined %s", thr->name);
        }
    }
    printf("\n");
    /* Student code below */  

    if (fork_flag == 1) {
        // fork
        printf("Added code to cleanup semaphores with a fork.\n");
        fflush(stdout);
        cpid = fork();
        if (cpid == 0) {
            // child
            cleanup();
        } else {
            char buf[BUFSZ];
            // int wstatus;
            wait(&status);
            zeroOutBuf(buf);
            // sprintf(buf, "ipcrm (%d) exited with status: %d\n", 
            //                         cpid, WEXITSTATUS(status)); 
            sprintf(buf, "ipcrm exited with status: %d\n", WEXITSTATUS(status)); 
            printf("%s",buf);
            exit(0);
        }

    } else {
        printf("Added code to cleanup semaphores.\n");
        fflush(stdout);
        cleanup();
    }

}

void cleanup() {
    char semid_str[20];
    /*sprintf(semid_str,"%d\0",semid);*/
    sprintf(semid_str,"%d%c", semid, '\0');
    char *newargv[] = { "/usr/bin/ipcrm", "-s", semid_str, NULL };
    char *newenv[] = { NULL };
    printf("Executing ipcrm.\n");
    execve(newargv[0], newargv, newenv);
}

void *thrfunc(void *p)
{
    /* THREAD FUNCTION */
    Runner *thr = p;
    int turns = 1;
    int tnum = thr->num;

    while (turns <= MAX_TURNS) {

        /* try to grab baton - only first thr can get it ; first thread then
         * releases second thread - and so on */

        semop(semid, baton_grab[tnum], 1);
        printf("[%d]", tnum);
        fflush(stdout); 

        /* release next thread */
        semop(semid, baton_release[tnum], 1); 
        turns++;
    }
    return (void *)0;
}


void zeroOutBuf(char *str)
{
	int i;
	for (i=0; i<BUFSZ; i++)
		str[i] = 0;
}