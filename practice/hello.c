#include <stdio.h>
//#include <stdbool.h>

void hello(void);
void plusOne(int *a);

int main(void) {

    printf("STDC VERSION: %ld\n", __STDC_VERSION__);

    hello();

    // define bool yourself or you could just include <stdbool.h>
    typedef int bool;
    const int true = 1;
    const int false = 0;

    bool x = true;

    if (x) {
        printf("x is true!\n");
    }   

    printf("an int uses %zu bytes of memory\n", sizeof(int));
    printf("an int* uses %zu bytes of memory\n", sizeof(int*));

    int i = 99;
    printf("i is %d\n", i);
    plusOne(&i);
    printf("i is now %d\n", i);


    printf("The address of i is %p\n", &i);
    

}

void hello(void)
{
    printf("Hello World!\n");
}

void plusOne(int *a)
{
    *a += 1;
}
