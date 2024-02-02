/*
Name: Michael Kausch
Class: Operating Systems
Professor: Gordon
Date: 2/1/24
Assignment: Lab 2
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>       /* to write time                 */
#include <sys/file.h>   /* open system call              */
#include <unistd.h>     /* header file for the POSIX API */
#include <sys/wait.h>   /* for wait system call          */
#include <fcntl.h>

#define BUFSZ 100

int fib(int n)
{
    if (n == 1 || n == 2)
        return 1;
    return (fib(n-1) + fib(n-2));
}

int oldfib(int n)
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

/* do this before using buffer for write calls */
void zeroOutBuf(char *str)
{
	int i;
	for (i=0; i<BUFSZ; i++)
		str[i] = 0;
}

int main(int argc, char *argv[], char *envp[])
{
    int num;
    int fd;
    pid_t pid;

    if (argc != 2) {
        printf("Usage: ./lab2 n\n");
        exit(1);
    }
    num = atoi(argv[1]);

    pid = fork();
    
    /*
    can get own pid like this:
    pid_t pid = getpid();  

    can get parent pid like this:
    pid_t parent = getppid();
    */

    if (pid == 0) {
        /* Child Process */
		char buf[BUFSZ];
		time_t timer;
		struct tm *tm_info;

		zeroOutBuf(buf);

        fd = open("log", O_WRONLY | O_CREAT | O_TRUNC, 0644);

        if (fd == -1) {
            printf("could not open log\n");
            exit(2);
        }

        /* write time to log */
		time(&timer);
		tm_info = localtime(&timer);
		strftime(buf, 26, "%Y:%m:%d %H:%M:%S\n", tm_info);
		write(fd, buf, strlen(buf));
        
        sprintf(buf, "fib %d = %d\n", num, fib(num)); 
        write(fd, buf, strlen(buf));
        close(fd);


        exit(num);

    } else {
        /* parent process */
		char buf[BUFSZ];
        int wstatus;
        pid_t child_pid;


        child_pid = wait(&wstatus);
		zeroOutBuf(buf);
        sprintf(buf, "my child (%d) exited with code: %d\n", 
                                child_pid, WEXITSTATUS(wstatus)); 
        printf("%s",buf);
        exit(0);
    
    }



    return 0;
}
