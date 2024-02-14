/*
Name: Michael Kausch
Class: Operating Systems
Professor: Gordon
Date: 2/14/23
Assignment: my_ipc
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
int i;

int main(void)
{
    char pathname[128];
    getcwd(pathname, 128);
    strcat(pathname, "/foo");
    
    // file to key
    // use a file to create a key
    int ipckey = ftok(pathname, 25);

    // setup shared memory
    // need a shared memory id - an int
    // comes from a function call to get the id
    int shmid = shmget(ipckey, sizeof(int), IPC_CREAT | 0666 );
    pid_t cpid = fork();
    
    if (cpid == 0) {
        // child
        // need to attach to the memory
        // second arg is supposed to be a void* so cast
        int * shared = shmat(shmid, (void*)0, 0);
        printf("child address: %p\n", shared);
        for (i=0; i<10; i++) {
            *shared = 10;
            //usleep(100);
            printf("child set 10 value: %d\n", *shared);
        }
        shmdt(shared);
        exit(0);
    } else {
        // parent
        shmat(shmid, (void*)0, 0);

        int * shared = shmat(shmid, (void*)0, 0);
        printf("parent address: %p\n", shared);
        usleep(80);
        for (i=0; i<10; i++) {
            *shared = 25;
           //usleep(100); 
            printf("parent set 25 value: %d\n", *shared);
        }
        // cleanup
        // detach from it
        int status;

        wait(&status);
        shmdt(shared);
        shmctl(shmid, IPC_RMID, 0);

    }

    return 0;
}
