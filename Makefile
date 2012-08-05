REVISION=$(shell svn info 2>/dev/null | grep Revision | cut -d' ' -f2)
ifeq ($(REVISION),)
	REVISION=0
endif

CC=gcc
BARE_CFLAGS=-fPIC -g -Wall
CFLAGS:=$(CFLAGS) $(BARE_CFLAGS) -DVERSION=$(REVISION)
LDFLAGS=-pthread
UNAME=$(shell uname)

EXECUTABLE=mukite

XMCOMP_OBJECTS=xmcomp/logger.o \
	xmcomp/xmlfsm.o \
	xmcomp/queue.o \
	xmcomp/cbuffer.o \
	xmcomp/sighelper.o \
	xmcomp/network.o \
	xmcomp/writer.o \
	xmcomp/reader.o \
	xmcomp/buffer.o \
	xmcomp/sha1/sha1.o

SOURCES=$(EXECUTABLE).c \
	parser.c \
	router.c \
	builder.c \
	jid.c \
	acl.c \
	config.c \
	rooms.c \
	room.c
OBJECTS=$(SOURCES:.c=.o)

all: $(EXECUTABLE)

run: $(EXECUTABLE)
	$(EXECUTABLE) config

$(EXECUTABLE): $(OBJECTS) $(XMCOMP_OBJECTS)

$(XMCOMP_OBJECTS):
	cd xmcomp ; $(MAKE)

clean:
	cd xmcomp ; $(MAKE) clean
	rm -f $(OBJECTS)
	rm -f $(EXECUTABLE)
