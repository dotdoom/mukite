REVISION=$(shell svn info 2>/dev/null | grep Revision | cut -d' ' -f2)
ifeq ($(REVISION),)
	REVISION=0
endif

CC=gcc
BARE_CFLAGS=-fPIC -g -Wall
CFLAGS:=$(CFLAGS) $(BARE_CFLAGS) -DVERSION=$(REVISION) -DLOG_PTHREAD -DLOG_CTIME
UNAME=$(shell uname)

SOURCES=writer.c reader.c logger.c network.c xmlfsm.c queue.c cbuffer.c sighelper.c buffer.c
OBJECTS=$(SOURCES:.c=.o)

all: $(OBJECTS) sha1/sha1.o

sha1/sha1.o:
	cd sha1 ; $(MAKE)

clean:
	cd sha1 ; $(MAKE) clean
	rm -f $(OBJECTS)
