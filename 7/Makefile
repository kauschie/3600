all: a.out dinephil

a.out: myls.c
	gcc myls.c -Wall

dinephil: dinephil.c
	gcc dinephil.c -Wall -lpthread -odinephil

clean:
	rm a.out
