/*
 *
 * Practice signals on lab3
 *
 */
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <stdlib.h>

volatile sig_atomic_t fd;
volatile sig_atomic_t sigusr1_flag = 0;
volatile sig_atomic_t sigusr2_flag = 0;
void usr1_handler(int sig);
void usr2_handler(int sig);
void term_handler(int sig);

int main(void)
{
    pid_t cpid;
    char msg[100];
    int wstatus;
    sigset_t mask;
    struct sigaction sa1, sa2, sa3;

    sigfillset(&mask);
    sigprocmask(SIG_BLOCK, &mask, NULL);

    /* set handler functions */
    sa1.sa_handler = usr1_handler; 
    sa2.sa_handler = usr2_handler;
    sa3.sa_handler = term_handler;

    /* set flags */
    sa1.sa_flags = 0;
    sa2.sa_flags = 0;
    sa3.sa_flags = 0;

    /* add signals that you want allowed while in the handler*/
    sigemptyset(&sa1.sa_mask);
    sigaddset(&sa1.sa_mask, SIGUSR2);

    sigemptyset(&sa2.sa_mask);
    sigaddset(&sa2.sa_mask, SIGTERM);

    sigfillset(&sa3.sa_mask);

    /* make calls to associate handler with sigaction struct*/
    if (sigaction(SIGUSR1, &sa1, NULL) == -1) {
        perror("sigaction SIGUSR1");
        exit(1);
    }
    if (sigaction(SIGUSR2, &sa2, NULL) == -1) {
        perror("sigaction SIGUSR2");
        exit(1);
    }
    if (sigaction(SIGTERM, &sa3, NULL) == -1) {
        perror("sigaction SIGTERM");
        exit(1);
    }

    // fork, get cpid, and check for errors
    if ((cpid = fork()) < 0) {
        perror("fork()");
        exit(1);
    }

    if (cpid == 0) {
        // child
		//fd = open("log", O_CREAT | O_WRONLY | O_TRUNC , 0644);
        fd = creat("log", 0644);
        sprintf(msg, "GO BAKERSFIELD");
        write(fd, msg, strlen(msg));

        sigfillset(&mask);
        sigdelset(&mask, SIGUSR1);

        sigsuspend(&mask);

        sprintf(msg, "ROADRUNNERS\n");
        write(fd, msg, strlen(msg));
        close(fd);
        exit(0);
    } else {
    // parent
        // signals don't go away and they stay queued, so the lab format was 
        // messing it up
        
//        kill(cpid, SIGTERM);
        write(1, "sending sigusr1\n", 17);
        kill(cpid, SIGUSR1);
        sleep(2);
        write(1, "sending sigusr2\n", 17);
        kill(cpid, SIGUSR2);
        sleep(2);
        write(1, "sending sigterm\n", 17);
        kill(cpid, SIGTERM);
        sleep(2);
        
        wait(&wstatus);

        char buf[100];

        if (WIFEXITED(wstatus)) {
            sprintf(buf, "Child terminated with exit code %d\n", WEXITSTATUS(wstatus));
        } else if (WIFSIGNALED(wstatus)) {
            sprintf(buf, "Child terminated by signal  %d\n", WSTOPSIG(wstatus));
        }   

        write(1, buf, strlen(buf));
        exit(0);
    }
}

void usr1_handler(int sig)
{
    char buf1[100];
    sprintf(buf1, " (got the signal and in usr1_handler) ");
    write(fd, buf1, strlen(buf1));
    write(1, "waiting for user2 signal.  ", 28);

    // create mask to suspend on 
    sigset_t mask;
    sigfillset(&mask); // ignore all signals
    sigdelset(&mask, SIGUSR2); // except SIGUSR2
    //sigprocmask(SIG_BLOCK, &mask, NULL); // tell it to block all those sigs

    sprintf(buf1, " suspending... ");
    write(1, buf1, strlen(buf1));

    sigsuspend(&mask); // suspend -- tthis will go to usr2_handler but
                       // the stack will unwind and come back here after
                       // usr1_handler -> usr2_handler -> term_handler -> usr2_handler -> usr1_handler

    sprintf(buf1, " done suspending in usr1_handler... ");
    write(1, buf1, strlen(buf1));
}


void usr2_handler(int sig)
{
    char buf1[100];
    sprintf(buf1, " (got the signal and in usr2_handler) ");
    write(fd, buf1, strlen(buf1));
    write(1, "waiting for term signal.  \n", 27);

    // fill it and delete the one for sigterm
    sigset_t mask;
    sigfillset(&mask);
    sigdelset(&mask, SIGTERM);
    //sigprocmask(SIG_BLOCK, &mask, NULL);

    sprintf(buf1, " suspending... ");
    write(1, buf1, strlen(buf1));

    sigsuspend(&mask);

    sprintf(buf1, " done suspending in usr2_handler... ");
    write(1, buf1, strlen(buf1));
}

void term_handler(int sig)
{
    char buf1[100];
    sprintf(buf1, " (got the signal and in term_handler) ");
    write(fd, buf1, strlen(buf1));
    // returns back to usr2_handler where it came from when the signal was issued
}
