# Makefile for examples week-10

all: lab10 maptest maptest2

maptest: maptest.c
	gcc maptest.c -Wall -omaptest -lpthread

maptest2: maptest.c
	gcc maptest2.c -Wall -omaptest2 -lpthread

lab10: rvlab10.c
	gcc rvlab10.c -Wall -olab10 -lpthread

clean:
	rm -f *.o lab10 maptest maptest2

run:
	./lab10 article log 10 20
