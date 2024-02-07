INCLUDE_DIRS = 
LIB_DIRS = 
CC=gcc

CDEFS=
CFLAGS= -O3 $(INCLUDE_DIRS) $(CDEFS)
LIBS= 

HFILES= 
CFILES= pthread.c

SRCS= ${HFILES} ${CFILES}
OBJS= ${CFILES:.c=.o}


all:	pthread

clean:
	-rm -f *.o *.d
	-rm -f perfmon pthread

distclean:
	-rm -f *.o *.d
	-rm -f pthread

pthread: pthread.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $@.o -lpthread

depend:

.c.o:
	$(CC) $(CFLAGS) -c $<
