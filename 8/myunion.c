#include <stdio.h>

int main(int argc, char * argv[], char* envp[])
{
    union {
        unsigned int colorref;
        unsigned char rgb[4]; 
    } color;

    color.colorref = 0x00ff0001;

    printf("%i ", color.rgb[0]);    // LSB
    printf("%i ", color.rgb[1]);
    printf("%i ", color.rgb[2]);
    printf("%i ", color.rgb[3]);    // MSB
                    //           MSB ----->  LSB
    printf("\n");   //            byte 3 2 1 0
    // printf("%i ", (color.colorref & 0xff000000) >> 24);
    // printf("%i ", (color.colorref & 0x00ff0000) >> 16);
    // printf("%i ", (color.colorref & 0x0000ff00) >> 8);
    // printf("%i ", (color.colorref & 0x000000ff));
    printf("%i ", (color.colorref & 0x000000ff));
    printf("%i ", (color.colorref & 0x0000ff00) >> 8);
    printf("%i ", (color.colorref & 0x00ff0000) >> 16);
    printf("%i ", (color.colorref & 0xff000000) >> 24);


    printf("\n");


    return 0;

}