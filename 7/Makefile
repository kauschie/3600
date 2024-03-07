all: a.out dinephil rvlab7

a.out: myls.c
	gcc myls.c -Wall

dinephil: dinephil.c
	gcc dinephil.c -Wall -lpthread -odinephil

rvlab7: rvlab7.c
	gcc rvlab7.c -Wall -lpthread -orvlab7

clean:
	rm a.out rvlab7 dinephil

run: 
	./rvlab7 40 10
