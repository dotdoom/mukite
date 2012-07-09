REVISION=$(shell svn info 2>/dev/null | grep Revision | cut -d' ' -f2)
ifeq ($(REVISION),)
	REVISION=0
endif

CC=gcc
BARE_CFLAGS=-fPIC -g -Wall
CFLAGS:=$(CFLAGS) $(BARE_CFLAGS) -DVERSION=$(REVISION) -DLOG_PTHREAD -DLOG_CTIME
LDFLAGS=-pthread
UNAME=$(shell uname)

LIBRARY=libmukite
WRAPPER=xmcomp/xmcomp

SOURCES=$(LIBRARY).c xmcomp/xmlfsm.c xmcomp/queue.c xmcomp/cbuffer.c xmcomp/sighelper.c parser.c router.c builder.c jid.c acl.c config.c rooms.c room.c
OBJECTS=$(SOURCES:.c=.o)

all: $(LIBRARY).so $(WRAPPER)

run: $(LIBRARY).so $(WRAPPER)
	LD_LIBRARY_PATH="." $(WRAPPER) <config

$(LIBRARY).so: $(OBJECTS)
	$(CC) -shared -Wl,-soname,$@.$(REVISION) -o $@.$(REVISION) $(OBJECTS)
	ln -sf $@.$(REVISION) $@

$(WRAPPER):
	cd xmcomp ; $(MAKE)

clean:
	cd xmcomp ; $(MAKE) clean
	rm -f $(OBJECTS)
	rm -f $(LIBRARY).so $(LIBRARY).so.*
