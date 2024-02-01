/* Name: Michael Kausch 
    Class: CMPS3600 - Operating Systems 
    Assignment: 
        Write a C program that: 
        opens a file,
        gets a number from the user,
        calculates which Fibonacci number is closest to the user's number,
        writes the number and Fibonacci number to the file,
        closes the file.
        Call: open, scanf, write, close
    Professor: Gordon Griesel
    Date: 1/22/24  
*/

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

int getClosestFib(long long int num)
{
    int i = 0;
    int j = 1;

    while (num > j) {
        int temp = j;
        j = i + j;
        i = temp;
    }

    return (num-i < j-num ? i : j);

}

int main(void)
{
    char filename[] = "fib_nums.txt";

    long long int user_input;

    int fp = open(filename, O_WRONLY | O_CREAT);

    if (fp == -1) {
        perror("Could not open fib_nums.txt... quitting now...");
        return 1;
    }

    printf("Enter a number: ");
    scanf("%lld", &user_input);

    long long int closest_fib = getClosestFib(user_input);
    printf("The closest fib number is %lld\n", closest_fib);
    printf("writing to file %s...", filename);
    /*fprintf(fp, "%d\n", closest_fib);*/
    char buf[512];
    sprintf(buf, "%lld\n", closest_fib);
    write(fp, buf, strlen(buf));
    printf(" finished writing file\n");

    int r = close(fp);
    if (r < 0) return 1;

    return 0;
}
