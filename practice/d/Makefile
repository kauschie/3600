# Makefile for examples week-13

all:	cond-wait timed-wait 
CC=gcc
CFLAGS=-lpthread 

timed-wait:	timed-wait.c
	gcc -o timed-wait timed-wait.c -Wall -lpthread

cond-wait:	cond-wait.c
	gcc -o cond-wait cond-wait.c -Wall -lpthread

nicetest:	nicetest.cpp
	g++ nicetest.cpp -Wall -o nicetest

row_reduce:	row_reduce.cpp
	g++ row_reduce.cpp -Wall -o row_reduce 

row_reduce_thr:	row_reduce_thr.c
	${CC} ${CFLAGS} row_reduce_thr.c -Wall -o row_reduce_thr -lm

clean:
	rm -f *.o nicetest row_reduce_thr row_reduce cond-wait timed-wait
