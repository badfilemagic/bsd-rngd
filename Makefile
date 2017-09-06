CFLAGS=-Wall -g

bsd-rngd:
	cc ${CFLAGS} main.c -o bsdrngd
clean:
	rm -f bsd-rngd *.o
