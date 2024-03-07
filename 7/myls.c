/*
Name: Michael Kausch
Class: Operating Systems
Professor: Gordon
Date: 3/4/24

Assignment: myls.c
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main()
{
    // system("ls -al");
    char *argv[] = {"/usr/bin/ls", "-al", NULL}; // NULL is a sentinel 
    execve("/usr/bin/ls", argv, NULL);
    printf("returning now!\n"); fflush(stdout);
    return 0;
}
