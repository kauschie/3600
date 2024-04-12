#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <wait.h>

int main()
{
    char buff[100];
    int cpid;
    int status;

    memset(buff, 13, 100);
    // create a pipe
    int fd[2]; // one for read and one for write
    pipe(fd);

    // child inherits open file descriptors
    // they will be the same files and reference the same things
    // whether they are open or closed depends on the process at that point
    // e.g. proc1 can close the read end but the read end will still be open in proc2

    if ((cpid = fork()) == 0) {
        // child
        close(fd[1]);
        // look at the pipe file descriptors
        printf("fd(child): %d %d\n", fd[0], fd[1]);

        // read from the pipe
        while(1) {
            int n = read(fd[0], buff, strlen(buff));
            if (n <= 0) break;
            buff[n] = '\0';
            printf("buf: **%s**\n", buff); fflush(stdout);
        }
        close(fd[0]);
    
    } else {
        // parent
        close(fd[0]);
        char pbuf[100] = {0};

        printf("fd(parent): %d %d\n", fd[0], fd[1]);
        scanf("%s", pbuf);
    
        // write to the pipe
        write(fd[1], pbuf, strlen(pbuf));
        close(fd[1]);
        wait(&status);
    }

    // close the pipe
    //close(fd[0]); close(fd[1]);



    return 0;
}