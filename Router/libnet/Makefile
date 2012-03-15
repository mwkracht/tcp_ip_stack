CC = gcc
CFLAGS = -Wall -g

all: library 

library: libnet.c libnet.h $(MAKEFILE)
	$(CC) $(CFLAGS) -c libnet.c
	rm -f libnet.a
	ar -r libnet.a libnet.o

clean:
	rm -rf *.o libnet.a
