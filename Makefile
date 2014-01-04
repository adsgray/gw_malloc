#
# Copyright (c) 2000 Andrew Gray 
# <agray@alumni.uwaterloo.ca>
#
#

CC=gcc

OBJS = hash.o malloc_track.o sem.o rwlock.o void_hash.o
TESTPOBJ = testp.o

CFLAGS = -g -Wall -Iinclude
#LDFLAGS += 

all: testp

mem.a: $(OBJS)
	ar -r mem.a $(OBJS)

testp: mem.a $(TESTPOBJ)
	$(CC) $(CFLAGS) -o testp $(TESTPOBJ) mem.a -lpthread

.PHONY: clean

clean:
	-rm $(OBJS) mem.a $(TESTPOBJ) testp
