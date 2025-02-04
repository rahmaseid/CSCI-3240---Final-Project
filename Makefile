CC=gcc
CFLAGS=-g -O1 -Wall
LDLIBS=-lpthread

all: client server

client: client.c csapp.c csapp.h
server: server.c csapp.c csapp.h

clean:
	rm -f *.o *~ *.exe client server csapp.o
