# Makefile for ppip

CC=gcc -m64
CFLAGS=-g -O -Wall

BINS=ppip
OBJS=ppip.o version.o

all:	$(BINS)

ppip: 	$(OBJS)
	$(CC) -o ppip $(OBJS)

ppip.o:		ppip.c
version.o:	version.c

version:
	git tag | sed -e 's/^v//' | awk '{print "char version[] = \"" $$1 "\";"}' >.version && mv .version version.c

clean:
	-rm -f *.o core $(BINS) *~ \#*

distclean:	clean
	-rm -f version.c

