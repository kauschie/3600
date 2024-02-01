#include <stdio.h>

int main(void)
{
    FILE *fp1, *fp2, *fp3;

    char name[1024];
    float length;
    int mass;

    int c;

    char s[1024];
    int linecount = 0;

    fp1 = fopen("whales.txt", "r");
    fp2 = fopen("hello.txt", "r");
    fp3 = fopen("quote.txt", "r");

    if (fp1 == NULL) {
        printf("could not open whales.txt");
        return 1;
    }

    if (fp2 == NULL) {
        printf("could not open hello.txt");
        return 1;
    }

    if (fp3 == NULL) {
        printf("could not open quote.txt");
        return 1;
    }
    //printf("opened whales.txt\n");

    // formatted input while skipping leading whitespace
    while (fscanf(fp1, "%s %f %d", name, &length, &mass) != EOF)
        printf("%s whale, %d tonnes, %.1f meters\n", name, mass, length);


    // reading it character by character
    // note that fgetc returns an int which is displayed as a char
    printf("\n\n");
    while ((c = fgetc(fp2)) != EOF)
        printf("%c", c);

    // line at a time and prints out a line number:
    while ((fgets(s, sizeof(s), fp3)) != NULL)
        printf("%d: %s", ++linecount, s);


    fclose(fp1);
    fclose(fp2);
    fclose(fp3);
}
