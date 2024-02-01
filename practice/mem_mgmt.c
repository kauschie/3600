#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void printArr(int *arr, int len)
{       
    for (int i = 0; i < len; i++) {
        printf("%d", arr[i]);
        if ((i+1)%10==0) {
            printf("\n");
        }
        else {
            printf(", ");
        }
    }
    printf("\n");
}

// reads a line of arbitrary size from a file
// returns a pointer to the line
// returns NULL on EOF or error
//
// up to the caller to free pointer when done with it
// strips the newline from the result
char* readline(FILE *fp)
{
    int offset = 0; // index nex char goes in the buff
    int buffsize = 4; // power of 2 initial size
    char *buf; // buffer
    int c;  // char we've read in

    buf = malloc(buffsize);

    if (buf == NULL)
        return NULL;

    while (c = fgetc(fp), c != '\n' && c != EOF) {
        // check if there's enough room in the buffer
        // account for the null term with -1
        if (offset == buffsize -1) {
            buffsize *= 2;
            char *new_buf = realloc(buf, buffsize);

            if (new_buf == NULL) {
                free(buf);  // on error free and bail
                return NULL;
            }   

            buf = new_buf;  // successful realloc
        }
        buf[offset++] = c;
    }

    // hit a newline or EOF at this point

    if (c == EOF && offset == 0) {
        // didn't read anything
        free(buf);
        return NULL;
    }

    if (offset < buffsize -1) {
        char * new_buf = realloc(buf, offset+1); // +1 for null terminator

        if (new_buf != NULL)
            buf = new_buf;
        else {
            free(buf);
            return NULL;
        }
    }

    // add NULL terminator
    buf[offset] = '\0';

    return buf;
}
        
int main(void)
{
    int *p = malloc(sizeof(int));
    *p = 12;
    printf("%d\n", *p);
    free(p);


    // allocation with error checking:
    if ((p = malloc(sizeof(int) * 10)) == NULL) {
        printf("Error allocating 10 ints\n");
        return 1;
    }

    for (int i = 0; i < 10; i++) {
        p[i] = i*5;
    }

    printArr(p, 10);
    free(p);

    // allocate space and zero all elements:
    //
    // Method 1: calloc zeros the memory
    int *arr1 = calloc(10, sizeof(int));
    
    // Method 2: memset
    int *arr2 = malloc(10 * sizeof(int));
    memset(arr2, 0, 10 * sizeof(int));

    printArr(arr1, 10);
    printArr(arr2, 10);

    // realloc will readjust buffer size 
    // does a copy only if the current location has changed

    free(arr1);
    free(arr2);

    FILE * fp = fopen("quote.txt", "r");
    if (fp == NULL) {
        printf("Could not open quote.txt... quitting now.\n");
        
    }
    char * line;

    while ((line = readline(fp)) != NULL) {
        printf("%s\n", line);
        free(line);
    }
    
    fclose(fp);


}
