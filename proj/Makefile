
all: phase1 phase2

phase1: phase1.c
	gcc phase1.c -Wall -lX11 -o phase1

phase2: phase2.c
	gcc phase2.c -Wall -lX11 -lpthread -o phase2 -DNUM_CHILDREN=20 -DNUM_BOXES=7
	gcc phase2.c -g -Wall -lX11 -lpthread -o dbg_phase2 -DNUM_CHILDREN=20 -DNUM_BOXES=15 -DDEBUG
	chmod 777 phase2


clean:
	rm -f phase1 phase2
	./cleanipc.sh mkausch
