BARE_CFLAGS=-fPIC -O2 -Wall
CFLAGS:=$(CFLAGS) $(BARE_CFLAGS) -DLOG_POS -DLOG_PTHREAD -DLOG_CTIME
UNAME=$(shell uname)

SOURCES=writer.c reader.c logger.c network.c xmlfsm.c queue.c cbuffer.c sighelper.c buffer.c
OBJECTS=$(SOURCES:.c=.o)
LIBRARY=xmcomp.o
EXTLIB=sha1/sha1.o

all: $(LIBRARY)
	
$(LIBRARY): $(OBJECTS) $(EXTLIB)
	ld -r $(OBJECTS) $(EXTLIB) -o $(LIBRARY)

sha1/sha1.o:
	cd sha1 ; $(MAKE)

clean:
	cd sha1 ; $(MAKE) clean
	rm -f $(OBJECTS) $(LIBRARY)
