
all: xwin89 fileio 

xwin89: xwin89.c
	gcc xwin89.c -Wall -lX11 -oxwin89

fileio: fileio.c
	gcc fileio.c -o fileio -Wall -ansi -pedantic

clean:
	rm -f xwin89 fileio 



