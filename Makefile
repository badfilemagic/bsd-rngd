CFLAGS=-Wall -g

bsd-rngd:
	cc ${CFLAGS} main.c -o bsd-rngd
clean:
	rm -f bsd-rngd *.o
