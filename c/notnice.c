#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int fib(int n) {
    if (n <= 2)
        return 1;
    else {
        return fib(n-1) + fib(n-2);
    }
}

int main()
{
    int count = 0;
    while (++count < 1200) {
        fib(29);
    }


    return 255;
}