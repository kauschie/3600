/*
Name: Michael Kausch
Class: Operating Systems
Professor: Gordon
Date: 1/31/24
Assignment: Lab 2
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <wait.h>

int main()
{
    pid_t pid = fork();

    if (pid == 0) {
        // Child Process
        sleep(1);
        while (1) { }
        exit(123);

    } else {
        // parent process
        sleep(1);
        int wstatus;
        wait(&wstatus);
    
    }



    return 0;
}
