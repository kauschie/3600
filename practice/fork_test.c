#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/types.h>

#define BUFFSIZE 100

int fib(int n)
{
    if (n == 1 || n == 2)
        return 1;
    return fib(n-1) + fib(n-2);
}

int main(int argc, char *argv[])
{
    char buffer[100];
    int val, fib_val;
    
    /* Zero Buffer */
    memset(buffer, 0, BUFFSIZE*sizeof(*buffer));

    if (argc != 2) {
        printf("Usage: ./fork n\n");
        exit(1);
    }

    val = atoi(argv[1]);

    pid_t pid = fork();

    if (pid == 0) {
        // child process
        int fd = open("log", O_WRONLY | O_TRUNC | O_CREAT, 0644);
        if (fd == -1) {
            perror("Unable to open file\n");
        }

        /* write info to child's log file */
		time_t timer;
		struct tm *tm_info;
		time(&timer);
		tm_info = localtime(&timer);
		strftime(buffer, 26, "%Y:%m:%d %H:%M:%S\n", tm_info);
		write(fd,buffer,strlen(buffer));

        fib_val = fib(val);
        sprintf(buffer, "Fib(%d): %d\n", val, fib_val);
        write(fd, buffer, strlen(buffer));

        close(fd);

        exit(val);

    } else {
        // parent
        int exit_status;
        wait(&exit_status);

        if (WIFEXITED(exit_status)) {
            printf("My child ( %d ) exited successfully with code %d\n", pid, WEXITSTATUS(exit_status));
        } 

        exit(EXIT_SUCCESS);

    }

    return 0;
}