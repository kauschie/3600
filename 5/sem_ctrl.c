/* filename: sem_ctrl.c
 *   Demonstrate use of semctl call to get info about a semaphore. For 
 *   background read manpage on semctl.
 *
 *   To compile and execute:
 *      $ gcc -o sem_ctrl sem_ctrl.c
 *      $ ./sem_ctrl
 *
 *   System V semaphore structures used in the call:
 *      struct sem {
 *         int semval;  # semaphore value - this is a non-negative integer 
 *         int sempid;  # PID of process that performed the last op on this sem
 *      };
 *
 *      struct sembuf {    
 *         unsigned short int sem_num;  # a semaphore num in the set: 0,1,2,... 
 *         short int sem_op;            # the semaphore operation 
 *         short int sem_flg;           # the operation flag 
 *      };    
 *
 *    sem_flg: A combination of the familiar IPC_NOWAIT flag, which indicates 
 *    if the test should return immediately or block until it passes, and 
 *    SEM_UNDO, which causes the semaphore operations to be undone if the 
 *    process should exit prematurely.
 *
 * Values for sem_op:
 *
 * If sem_op is 0, sem_num is tested to see if its value is 0; if so, the 
 * next test runs. If sem_num is not 0, either the operation blocks until the 
 * semaphore becomes 0 if IPC_NOWAIT is not set, or the rest of the tests are 
 * skipped if IPC_NOWAIT is set.
 *
 * If sem_op is a positive integer, the value of sem_op is added to the value 
 * of the semaphore.
 *
 * If sem_op is a negative integer and the value of the semaphore is greater 
 * than or equal to the absolute value of sem_op, then the absolute value is 
 * subtracted from the value of the semaphore.
 *
 * If sem_op is a negative integer and the value of the semaphore is less than 
 * the absolute value of sem_op, then the execution of tests immediately stops 
 * if IPC_NOWAIT is true, or blocks until the semaphore's value becomes greater
 * than the absolute value of sem_op if false.   
 */

#include <sys/types.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <string.h> /* For strerror(3c) */
#include <errno.h>  /* For errno */
#include <unistd.h> /* rand(3c) */
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>  /* exit(3) */


union {
	int val;
	struct semid_ds *buf;
	unsigned short *array;
	struct seminfo *__buf;
} my_semun;

struct sembuf grab[2], release[1];
int n_waiting;   /* num processes waiting for the semaphore */
int sem_value;   /* the value of the semaphore */
pid_t lastpid;   /* process that performed last semop */
char pathname[200];
key_t ipckey;
int semid;

struct sigaction sa;

void ctrlc_handler (int dummy)
{
	printf("ctrlc_handler()...\n");
   	/* release the semaphore now */
	printf("   Release the semaphore now!\n");
	semop(semid, release, 1);
	sem_value = semctl(semid, 0, GETVAL);
	lastpid = semctl(semid, 0, GETPID);
	printf("   Semaphore value after release: %d\n", sem_value);
	printf("   Handler is returning.\n");
}

int main(int argc, char *argv[])   
{
	printf("\nsem_ctr program has started.\n");
	printf("testing the control of a semaphore's value.\n\n");

	printf("Setup a signal handler.\n");
	/* setup a handler so that ctrl-c can release the semaphore */
	sa.sa_handler = ctrlc_handler;
	sigaction(SIGINT, &sa, NULL);

	printf("Get an IPC key.\n");
	getcwd(pathname,200);
	strcat(pathname,"/foo");
	ipckey = ftok(pathname, 42);

	printf("Get a semaphore set.\n");
	/* get a semaphore set with 1 semaphore in it (i.e., sem 0) */
	int nsem = 1;
	semid = semget(ipckey, nsem, 0666 | IPC_CREAT);
	if (semid < 0) {
		printf("Error - %s\n", strerror(errno));
		_exit(1);
	}

	/* IMPORTANT note from semget manpage:
	 * The values of the semaphores in a newly created set are indeterminate.
	 * (POSIX.1-2001 is explicit on this point.) Although Linux, like many
	 * other implementations, initializes the semaphore values to 0, a	porta-
	 * ble application cannot rely on this: it should explicitly initialize
	 * the semaphores to the desired values.
	 */ 

	/* setup GRAB semaphore operation
	 * there are two steps involved
	 * first, check if value is zero
	 *    if so grab it
	 * otherwise
	 *    wait for zero
	 * These operations are executed atomically
	 *                               ----------
	 */

	/* setup the GRAB semaphore operation */
	/* apply an operation to semaphore 0 in the set */
	/* first, tell it which semaphore */ 
	grab[0].sem_num = 0;
	grab[1].sem_num = 0;
	/* next, let it release semaphore if holder dies */ 
	grab[0].sem_flg = SEM_UNDO;
	grab[1].sem_flg = SEM_UNDO;
	/* here are the 2 operations */
	/* wait until value is zero */
	grab[0].sem_op = 0;
	/* increment sem value by 1 */
	grab[1].sem_op = 1;

	/* setup RELEASE semaphore operation */
	/* apply the operation to semaphore 0 in the set */
	release[0].sem_num = 0;
	/* release semaphore if holder dies */ 
	release[0].sem_flg = SEM_UNDO;
	/* decrement semaphore by -1 to unlock it */
	/* Actually, we add -1 to the semaphore value,
	 * which is like a decrement. */
	release[0].sem_op = -1;

	/* you can set the semaphore to any positive number */
	printf("\nset semaphore value to 0...\n");
	my_semun.val = 0;
	semctl(semid, 0, SETVAL, my_semun);
	sem_value = semctl(semid, 0, GETVAL);
	printf("sem value = %d  pid: %d\n", sem_value, lastpid);
	if (sem_value == 0)
		printf("Success!\n");
	else
		printf("Error, value not zero.\n");


	printf("\nDoing the grab operation...\n");
	/* The grab operation was defined just above. */
	/* Now, perform the grab operation. */
	/* 2 means perform 2 operations in the sembuf struct */
	semop(semid, grab, 2);

	printf("Look at the status.\n");
	/* This lets you know status of queue */
	n_waiting = semctl(semid, 0, GETZCNT);
	sem_value = semctl(semid, 0, GETVAL);
	lastpid = semctl(semid, 0, GETPID);
	printf("sem value = %d  pid: %d\n", sem_value, lastpid);
	if (sem_value == 1)
		printf("Success!\n");
	else
		printf("Error, sem value is not 1\n");

	printf("\nTest the IPC_NOWAIT functionality...\n");
	/* Try to grab it again - this time don't wait */
	/* We don't wait because we already know that the value is not zero. */
	/* This is just to test out the IPC_NOWAIT functionality. */
	grab[0].sem_flg = IPC_NOWAIT;
	if (semop(semid, grab, 2) < 0)
		printf("\nDid not grab semaphore! Not waiting around for zero.\n");
	else
		printf("\nDid grab semaphore. Something is wrong.\n");

	sem_value = semctl(semid, 0, GETVAL);
	lastpid = semctl(semid, 0, GETPID);
	printf("sem value = %d  pid: %d\n", sem_value, lastpid);

	printf("\nSending a SIGINT value to your own process.\n");
	/* send a signal to yourself to release semaphore */
	kill(getpid(), SIGINT);

	printf("\nRemoving the semaphore.\n");
	/* remove semaphore */
	if ((semctl(semid, 0, IPC_RMID)) < 0) {
		perror("semctrl IPC_RMID");
		exit(EXIT_FAILURE);
	} 
	printf("\nProgram ending successfully.\n");
	exit(EXIT_SUCCESS);
}







