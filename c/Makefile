
all: phase3

phase3: phase3.c
	gcc phase3.c -g -Wall -lX11 -lpthread -o phase3 -DNUM_BOXES=5

clean:
	rm -f phase3
	./cleanipc.sh mkausch
