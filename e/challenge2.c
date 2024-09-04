#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

typedef struct {
    double x,y;
} Vec2;

Vec2 getRandPoint();
double getRad(Vec2 loc);

// int width = 100000000;
// int height = 100000000;

int main(int argc, char* argv[]) {

    srand(time(NULL));
    int MAX = 10000000;
    int num_in = 0;


    // Vec2 test = {3, 4};
    // printf("test hypoteneus: %f\n", getRad(test));

    for (int i = 0; i < MAX; i++) {
        Vec2 p = getRandPoint();
        if (getRad(p) < 1.0) {
            num_in++;
            // printf("got one\n");
        }
    }

    double result = ((double)(num_in) / MAX)*4;

    printf("result = %f\n", result);
    //printf("num_in: %d\n", num_in);

    return 0;
}


void printArr(int *arr, int size) {
    for (int i = 0; i < size; i++) {
        printf("%d ", arr[i]);
    }
}

Vec2 getRandPoint() {
    double x = ((double)rand()) / RAND_MAX;
    // usleep(7);
    double y = ((double)rand()) / RAND_MAX;
    Vec2 p = {x, y};

    return p;
}

double getRad(Vec2 loc) 
{
    return sqrt(pow(loc.x, 2)+ pow(loc.y, 2));
}
