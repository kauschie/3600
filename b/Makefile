# Makefile for examples week-10

all: pipetest dotprod pgflt_test cache_check test rlimit rvlab11

pipetest: pipetest.c
	gcc pipetest.c -Wall -opipetest -lpthread

dotprod: dotprod.c
	gcc dotprod.c -Wall -odotprod -lpthread

rvlab11: rvlab11.c
	gcc rvlab11.c -Wall -orvlab11 -lpthread

pgflt_test: pgflt_test.c
	gcc pgflt_test.c -Wall -opgflt_test

cache_check: cache_check.c
	gcc cache_check.c -Wall -ocache_check

test: test.c
	gcc test.c -Wall -otest

rlimit: rlimit.c
	gcc rlimit.c -Wall -orlimit

clean:
	rm -f *.o pipetest dotprod pgflt_test cache_check
	rm -f test rlimit a.out

