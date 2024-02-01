/*
   fork4.c     

   Demonstrate how a child becomes an orphan.

   When the parent dies before harvesting the child's exit code
   the child becomes an orphan and is inherited by init.
   init harvests the code (no zombie)

   Compile, execute and view log file:

   $ make  
   OR 
   $ gcc fork4.c -Wall -ofork4
   $ ./fork4
   $ cat log
   */

#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

int fib(int n)
{
	int a = 1, b = 1, c;
	if (n == 1)
		return a;
	if (n == 2)
		return b;
	while (n > 2) {
		c = a + b;
		a = b;
		b = c;
		--n;
	}
	return c;
}

int main(void)
{
	int ret;
	pid_t cpid, parent;
	parent = getppid();

	//#define TEST_FIB
	#ifdef TEST_FIB
	int i;
	for (i=1; i<12; i++) {
		printf("fib %i: %i\n", i, fib(i));
	}
	return 0;
	#endif //TEST_FIB

  	/* fork returns pid of child to the parent, 0 to child */
	cpid = fork();

	if (cpid == 0) {
		/* 0 means you are the child */
		/* Yes this is the child process. */

		char buf[100];
		int logfd;   
		pid_t me;
		me = getpid();
		parent = getppid();

	  	/* open a log */
		logfd = open("log", O_WRONLY|O_CREAT|O_TRUNC, 0644);

		/* write pids to log */
		sprintf(buf, "me: %d my parent: %d\n", me, parent); 
		write(logfd, buf, strlen(buf));

		/* child suspends for a few seconds to make sure parent is dead */
		sleep(2);

		/* grab pids after wait - parent should be init */
		sprintf(buf,"me: %d my parent: %d\n", getpid(), getppid()); 
		write(logfd, buf, strlen(buf));

		/* sync the file or strange things happen when the parent dies */
		fsync(logfd);
		close(logfd);
		exit(0); 
	} else {
		/* PARENT process */
		/* parent does some busy work before dying
		   long enough for the child to write its first entry to log
		*/
		srand((unsigned)time(NULL));
		int r = rand() % 9 + 10;
		ret = fib(r); /* busy work */
		printf("Parent pid: %d  %ith Fibonacci number: %d\n", parent, r, ret);
		exit(0);
	}
}

