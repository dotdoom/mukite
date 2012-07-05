#ifndef ROUTER_H
#define ROUTER_H

#include "jid.h"
#include "xmcomp/xmlfsm.h"
#include "builder.h"
#include "room.h"

#define MAX_ERASE_CHUNKS 5

typedef struct {
	char name, type;

	Jid real_from, proxy_to;

	BufferPtr header, inner;

	// buffer chunks to be erased (from, to, type and muc#user in presence)
	BufferPtr erase[MAX_ERASE_CHUNKS];

	Room *room;
} IncomingPacket;

typedef int(*SendProc)(void *, BuilderPacket *);

typedef struct {
	SendProc proc;
	void *data;
} SendCallback;

int route(IncomingPacket *, SendCallback *, char *);

#endif
