/*
Name: Michael Kausch
Class: Operating Systems
Professor: Gordon
Date: 1/31/24
Assignment: Lab 2
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

void handle_the_signal(int sig)
{
    printf("we got a SIGINT signal!!!\n");
    exit(0);
}


int main(void)
{
    int count = 0;

    signal(SIGINT, handle_the_signal);
    while (1) {
        printf("hello %d\r", count++);
        fflush(stdout);
        sleep(1);
    }

    return 0;
}
