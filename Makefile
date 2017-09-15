CFLAGS=-Wall -g 

bsd-rngd:
	cc ${CFLAGS} bsdrngd.c -o bsdrngd
clean:
	rm -f bsd-rngd *.o
