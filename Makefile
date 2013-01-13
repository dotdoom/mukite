BARE_CFLAGS=-fPIC -O2 -Wall
CFLAGS:=$(CFLAGS) $(BARE_CFLAGS) -DLOG_POS -DLOG_PTHREAD -DLOG_CTIME -DMEWCATE
LDFLAGS=-pthread

EXECUTABLE=mukite

XMCOMP=xmcomp/xmcomp.o
SOURCES=$(EXECUTABLE).c \
	worker.c \
	timer.c \
	component.c \
	packet.c \
	builder.c \
	jid.c \
	acl.c \
	config.c \
	rooms.c \
	room.c \
	mewcate.c
OBJECTS=$(SOURCES:.c=.o)

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS) $(XMCOMP)

$(XMCOMP):
	cd xmcomp ; $(MAKE)

clean_mukite:
	rm -f $(OBJECTS)
	rm -f $(EXECUTABLE)

clean: clean_mukite
	cd xmcomp ; $(MAKE) clean
