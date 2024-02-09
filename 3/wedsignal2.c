/*
Name: Michael Kausch
Class: Operating Systems
Professor: Gordon
Date: 2/7/24
Assignment: signal2 class code-along
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <wait.h>


void handle_the_signal(int sig)
{
    printf("we got a SIGINT signal!!!\n");
    exit(0);
}


int main(void)
{
    int status;
    pid_t cpid = fork();

    if (cpid == 0) {
        // child

        while (1) { break; }
        exit(98);

    } else {
        // parent
        usleep(40);
        kill(cpid, SIGINT);
        wait(&status);

        if (WIFSIGNALED(status))
            psignal(WTERMSIG(status), "child was terminated!");
        else if (WIFEXITED(status)) 
            printf("child exited: %i\n", WEXITSTATUS(status));

    }

    return 0;
}
