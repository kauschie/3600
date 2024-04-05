
all: lab9 lab9a

lab9: rvlab9.c
	gcc rvlab9.c -Wall -lX11 -lXext -lpthread -olab9

lab9a: rvlab9a.c
	gcc rvlab9a.c -g -Wall -lX11 -lXext -lpthread -olab9a
	gcc rvlab9a.c -g -Wall -lX11 -lXext -lpthread -olight -DUSELIGHT=1

clean:
	rm lab9 lab9a light
	./cleanipc.sh mkausch
