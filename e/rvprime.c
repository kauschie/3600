#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>

int digits[11];
int primes[100000000];
int idx = 0;
int numDigits = 0;

int isPrime(int n);
void breakupNum(int n);
int arrsum(int n);
void printArr(int *arr, int size);



int main(int argc, char* argv[]) {

    int startingNum = 2;
    int numPrime = 0;
    int targetSum = 59;
    int cutoff = 20;

    int val = startingNum;
    while (numPrime < cutoff) {
        breakupNum(val);
        // printArr(digits, numDigits);
        // getchar();
        if (arrsum(numDigits) == targetSum) {
            if (isPrime(val)) {
                printf("found prime that adds to %d: %d\n", targetSum, val);
                numPrime++;
            }
        }
        if (numPrime == cutoff) break;
        val++;
    }
        

    printf("%dth prime adding to %d is: %d\n", cutoff, targetSum, val);


    return 0;
}

int isPrime(int n) {
    int result = 1;
    
    if (n <= 3)
        return 1;

    int start = sqrt((double)n);
    // if (start%2 == 0) return 0;

    for (int i = start; i > 1; i--) {
        if (n%i == 0) {
            result = 0;
            break;
        }
    }

    return result;
}

void breakupNum(int n)
{
    // printf("in breakupNum\n");
    int remainingNum = n;
    numDigits = 0;
    while (remainingNum > 0) {
        digits[numDigits] = remainingNum % 10;
        numDigits++;
        // printf("numDigits: %d\n", numDigits);
        remainingNum = remainingNum/10;
        // printf("remainingNum: %d\n", remainingNum);
        // getchar();
    }
}

int arrsum(int n)
{
    // printf("in arrsum\n");
    int sum = 0;
    for (int i = 0; i <= n; i++) {
        sum += digits[i];
    }

    // printArr(digits, numDigits);
    return sum;

}

void printArr(int *arr, int size) {
    for (int i = 0; i < size; i++) {
        printf("%d ", arr[i]);
    }
}