
all: phase1 phase2

phase1: phase1.c
	gcc phase1.c -Wall -lX11 -o phase1

phase2: phase2.c
	gcc phase2.c -Wall -lX11 -lpthread -o phase2 -DNUM_CHILDREN=20 -DNUM_BOXES=8

clean:
	rm -f phase1 phase2
