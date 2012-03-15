JAVAC=$(HOME)/jdk1.5.0/bin/javac
JAVAC=javac

CFLAGS=-Wall -Ilibnet -g
LDLIBS=-Llibnet -lnet -lpthread

all: libnet/libnet.a $(MAKEFILE)


libnet/libnet.a:
	(cd libnet; $(MAKE))


