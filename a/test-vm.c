/* test-vm.c
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>

#define SIZE 200

/* allocate memory for a static matrix */
int matrix[SIZE][SIZE];

int main()
{
   int stuff = 99; /* this variable will be on the stack */
   /* allocate memory on the stack */
   int mystack[SIZE*1000];


   /* allocate memory for a stack dynamic vector */
   int *vptr = malloc(SIZE*sizeof(int)); 
   printf("PID: %d\n",getpid());


   char *dest = mmap(NULL, SIZE, PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,-1,0);
   printf("map address: %p\n",dest);

   /* set break 30 to break at this line */ 
   while(1) {
      vptr[0] = 1;
      matrix[SIZE-1][SIZE-1] = 1;
   }
   free(vptr);
   return 0;
}

