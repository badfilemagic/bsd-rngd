CFLAGS=-Wall -g -DHARDENEDBSD -O2 

bsd-rngd:
	cc ${CFLAGS} bsdrngd.c -o bsdrngd
clean:
	rm -f bsdrngd *.o
