/*
 *  Name: Michael Kausch
 *	Date: 2/8/24
	Assignment: Lab 3
	Professor: Gorden
	Class: Operating Systems
 *
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

void display_status(int *);
void sigint_handler(int sig);

/* these need to be global if you want the handler can see them */
int status;  
pid_t parent, cpid;
int fd;

int main(void)
{
	int wstatus;
	struct sigaction sa;
	sigset_t mask;
	char msg[100];

	sigfillset(&mask);
	sigprocmask(SIG_BLOCK, &mask, NULL);
	sigaddset(&mask, SIGUSR1);


	/* setup Ctrl-C handler before the fork */ 
	sa.sa_handler = sigint_handler;
	// sa.sa_flags = 0; 
	sa.sa_flags = SA_RESTART; /* restart system calls if interrupted */ 
	sigfillset(&sa.sa_mask);  /* mask all signals while in the handler */
	sigprocmask(SIG_BLOCK, &sa.sa_mask, NULL);


	/* register the handler  */
	if (sigaction(SIGINT, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}  

	

	parent = getpid(); 
	cpid = fork();   

	if (cpid == 0) {
		/* CHILD process */
		/* put the child in an infinite loop */
		sigdelset(&sa.sa_mask, SIGUSR1);
		fd = open("log", O_CREAT | O_WRONLY | O_TRUNC , 0644);
		sprintf(msg, "Go Bakersfield ");
		write(fd, msg, strlen(msg));

		sigsuspend(&mask);	// wait with mask that allows sigusr1
		write(fd, "Roadrunners", 11);
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
	sprintf(msg, "(got the signal)\n");
	write(fd, msg, strlen(msg));

}



