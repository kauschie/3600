#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
int main(void)
{
    char buf[100];
    char buf2[100];

    sprintf(buf, "Enter a 2-digit number: ");
    write(1, buf, strlen(buf));

    read(0, buf2, 3);
    int num = atoi(buf2);
    printf("you read in %d\n", num);

}
