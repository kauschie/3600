# Makefile for week-4

all: shared_mem shm_io sigset sigset2 strings rvlab4 xwin89 proj phase1

xwin89: xwin89.c
	gcc xwin89.c -Wall -lX11 -oxwin89

proj: xwin89-proj.c
	gcc xwin89-proj.c -Wall -lX11 -oproj

phase1: phase1.c
	gcc phase1.c -Wall -lX11 -ophase1

shared_mem: shared_mem.c
	gcc shared_mem.c -Wall -oshared_mem

shm_io: shm_io.c
	gcc shm_io.c -Wall -oshm_io

sigset: sigset.c
	gcc sigset.c -Wall -osigset

sigset2: sigset2.c
	gcc sigset2.c -Wall -osigset2

strings: strings.c
	gcc strings.c -Wall -ostrings

rvlab4: rvlab4.c
	gcc rvlab4.c -Wall -orvlab4

clean:
	rm -f shared_mem shm_io sigset sigset2 strings phase1 proj

run:
	./rvlab4 < myinput; echo "PRINTING LOG\n"; cat log; echo "DONE PRINTING LOG\n";
	./rvlab4; echo "PRINTING LOG\n"; cat log; echo "DONE PRINTING LOG\n";
