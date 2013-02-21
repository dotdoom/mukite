SRCDIR:=src
SOURCES:=$(addprefix $(SRCDIR)/, \
	affiliation/affiliation.c \
	affiliation/affiliations.c \
	participant/participant.c \
	participant/participants.c \
	history_entry/history_entry.c \
	history_entry/history_entries.c \
	acl.c \
	builder.c \
	component.c \
	config.c \
	jid.c \
	mewcat.c \
	mukite.c \
	packet.c \
	room/room.c \
	room/rooms.c \
	timer.c \
	worker.c)
OBJDIR:=obj
OBJECTS:=$(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
BINDIR:=bin
BINARY:=$(BINDIR)/mukite
EXTLIB:=xmcomp/bin/xmcomp.o
LDFLAGS:=-pthread

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
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(dir $<) -c $< -o $@

$(EXTLIB):
	cd xmcomp ; $(MAKE)

clean:
	cd xmcomp ; $(MAKE) clean
	rm -rf $(BINDIR) $(OBJDIR)
