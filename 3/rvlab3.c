/*
 *  Name: Michael Kausch
 *	Date: 2/8/24
	Assignment: Lab 3
	Professor: Gorden
	Class: Operating Systems
 
 *          struct sigaction {
 *               void     (*sa_handler)(int);
 *               void     (*sa_sigaction)(int, siginfo_t *, void *);
 *               sigset_t   sa_mask;
 *               int        sa_flags;
 *               void     (*sa_restorer)(void);
 *         };
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/file.h>

void sigint_handler(int sig);

volatile sig_atomic_t status;  
volatile pid_t parent, cpid;
volatile sig_atomic_t fd;

int main(void)
{
	int wstatus;
	struct sigaction sa;
	sigset_t mask;
	char msg[100];

	sigfillset(&mask);
	sigprocmask(SIG_BLOCK, &mask, NULL);
    sigdelset(&mask, SIGUSR1);

	sa.sa_handler = sigint_handler;
    
	sa.sa_flags = 0; 
	sa.sa_flags = SA_RESTART; /* restart system calls if interrupted */ 

    // sets mask for when in signal handler
	sigfillset(&sa.sa_mask);  /* mask all signals while in the handler */
	sigprocmask(SIG_BLOCK, &sa.sa_mask, NULL);


	/* register the handler  */
	if (sigaction(SIGUSR1, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}  

	parent = getpid(); 
	cpid = fork();   

	if (cpid == 0) {
		/* CHILD process */
		fd = open("log", O_CREAT | O_WRONLY | O_TRUNC , 0644);
		sprintf(msg, "Go Bakersfield");
		write(fd, msg, strlen(msg));

		sigsuspend(&mask);	// wait with mask that allows sigusr1
		write(fd, "Roadrunners\n", 12);
		close(fd);
		exit(0);

	} else {
		/* PARENT process */

		kill(cpid, SIGTERM);
		kill(cpid, SIGUSR1);
		wait(&wstatus);
		if (WIFSIGNALED(wstatus)) { 
			sprintf(msg, "child terminated by signal %d\n", WTERMSIG(wstatus));
		} else {
			sprintf(msg, "child terminated with exit code %d\n", WEXITSTATUS(wstatus));
		}
		write(fd, msg, strlen(msg));

	}

	return 0;
}

/* SIGINT HANDLER */
void sigint_handler(int sig)
{
	char msg[100];
	sprintf(msg, " (got the signal) ");
	write(fd, msg, strlen(msg));
}



