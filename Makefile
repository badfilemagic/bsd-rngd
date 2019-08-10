CFLAGS=-Wall -g -DHARDENEDBSD -O2 -lutil 

bsd-rngd:
	cc ${CFLAGS} bsdrngd.c -o bsdrngd
clean:
	rm -f bsdrngd *.o
