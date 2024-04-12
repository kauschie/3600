
/*
 * Name: Michael Kausch
 * Assignment: Lab4
 * Class: Operating Systems
 * Professor: Gordon
 * Date: 2/15/2024
 */

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>

int status;

int main(void)
{
    // make pathname to local file foo for ipckey generation
    char pathname[128];
    getcwd(pathname, 128);
    strcat(pathname, "/foo");   /* make sure file foo is in your directory*/
    int shmid, mqid;

    // for ipc messages
    struct {
        long type; // first must be a long
        char text[100];
    } mymsg;

    // generate ipckey, actually just an int but key_t makes it portable
    key_t ipckey = ftok(pathname, 69);   

    // check for failures in generating key
    if (ipckey == -1) {
        perror("ipckey error: ");
        exit(EXIT_FAILURE);
    } 

    // check for failures when getting msg queue id
    mqid = msgget(ipckey, IPC_CREAT | 0666);
    if (mqid < 0) {
        perror("msgget");
        exit(EXIT_FAILURE);
    }

    //printf("Message queue identifier is %d\n", mqid);

    int fd = open("log", O_CREAT | O_WRONLY | O_TRUNC , 0644);
    if (fd < 0) {
        perror("open");
        exit(EXIT_FAILURE);
    }   

    // get shared mem id
    shmid = shmget(ipckey, sizeof(int), IPC_CREAT | 0666); 

    if (shmid < 0) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    // attach to shared memory and get the pointer
    int *shared = (int*)shmat(shmid, (void *)0, 0);
    *shared = 0;

    // fork and get the child
    pid_t child = fork();

    if (child == 0) {
        /* child  */
        
        // sleep until shared is no longer 0
        while (*shared == 0) {
            usleep(1000);
        }

        // write value of shared to log now that its changed
        char buf[100];
        sprintf(buf,"%d\n",*shared);
        write(fd, buf, strlen(buf));

        // block and receive message from parent then write
        msgrcv(mqid, &mymsg, sizeof(mymsg), 1, 0); /* blocks on queue */ 
        write(fd, mymsg.text, strlen(mymsg.text));

        // cleanup
        close(fd);
        shmdt(shared);
        exit(0);

    } else {
        /* Parent */
        char buf[100];
        int val, wstatus;
        sprintf(buf,"Enter a 2-digit number: ");
        write(1,buf,strlen(buf));

        memset(buf, 0, 100);
        read(0, buf, 3);
        val = atoi(buf);
        *shared = val;

        // prompt the user for a sentence
        sprintf(buf,"Enter a word: ");
        write(1,buf,strlen(buf));

        // get the message
        memset(buf, 0, 100);
        read(0, buf, 100);

        // build mymsg struct
        memset(mymsg.text, 0, 100);
        strcpy(mymsg.text, buf);
        mymsg.type = 1;

        // send the message
        msgsnd(mqid, &mymsg, sizeof(mymsg), 0);

        wait(&status); /* wait for the child to die */
        shmdt(shared);

        if (WIFEXITED(wstatus)) {
            sprintf(buf, "Child terminated with exit code %d\n", WEXITSTATUS(wstatus));
            write(1, buf, strlen(buf));
        }

        // clean up shared memory
        shmctl(shmid, IPC_RMID, 0);  
        // clean up message queue
        msgctl(mqid, IPC_RMID, 0);


    }

    return 0;

}


