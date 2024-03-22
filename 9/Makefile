
all: lab9 phase3

lab9: rvlab9.c
	gcc rvlab9.c -Wall -lX11 -lXext -lpthread -olab9

phase3: rvlab9-sem.c
	gcc rvlab9-sem.c -g -Wall -lX11 -lXext -lpthread -ophase3
	gcc rvlab9-sem.c -g -Wall -lX11 -lXext -lpthread -olight -DUSELIGHT=1

clean:
	rm lab9 phase3
	./cleanipc.sh mkausch