#include <stdio.h>
#include <stddef.h> // for using offsetof

struct cabin_information {
    int window_count;
    int o2level;
};

struct spaceship {
    char *manufacturer;
    struct cabin_information ci;
};


struct foo {
    int a;
    char b;
    int c;
    char d;
};

int main(void)
{
    struct spaceship s = {
        .manufacturer="General Products",
        .ci.window_count = 8,
        .ci.o2level = 21
    };

    struct spaceship s1 = {
        .manufacturer="General Products 2",
        .ci={
            .window_count = 9,
            .o2level = 35
        }

    };

    printf("%s: %d seats, %d%% oxygen\n",
            s.manufacturer, s.ci.window_count, s.ci.o2level);

    printf("%s: %d seats, %d%% oxygen\n",
            s1.manufacturer, s1.ci.window_count, s1.ci.o2level);


    printf("%zu\n", offsetof(struct foo, a));
    printf("%zu\n", offsetof(struct foo, b));
    printf("%zu\n", offsetof(struct foo, c));
    printf("%zu\n", offsetof(struct foo, d));


    return 0;
}
