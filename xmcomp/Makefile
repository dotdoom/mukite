SRCDIR:=src
SOURCES:=$(addprefix $(SRCDIR)/, \
	writer.c \
	reader.c \
	logger.c \
	network.c \
	xmlfsm.c \
	queue.c \
	ringbuffer.c \
	sighelper.c \
	buffer.c)
OBJDIR:=obj
OBJECTS:=$(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
BINDIR:=bin
BINARY:=$(BINDIR)/xmcomp.o
EXTLIB:=sha1/sha1.o

override CFLAGS+=-I. -I$(SRCDIR) -fPIC -Wall -std=gnu99 -DLOG_POS -DLOG_PTHREAD -DLOG_CTIME

all: release

release:
	$(MAKE) $(BINARY) CFLAGS='-O2 -march=native'

debug:
	$(MAKE) $(BINARY) CFLAGS='-g'
	
$(BINARY): $(OBJECTS) $(EXTLIB)
	mkdir -p $(BINDIR)
	ld -r $(OBJECTS) $(EXTLIB) -o $(BINARY)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(EXTLIB):
	$(MAKE) -C sha1

clean:
	$(MAKE) -C sha1 clean
	rm -rf $(BINDIR) $(OBJDIR)
