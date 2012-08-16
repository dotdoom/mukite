#ifndef ROUTER_H
#define ROUTER_H

#include "xmcomp/xmlfsm.h"

#include "jid.h"
#include "builder.h"
#include "config.h"

#define MAX_ERASE_CHUNKS 5

typedef struct {
	char name, type;

	Jid real_from, proxy_to;

	BufferPtr header, inner;

	// buffer chunks to be erased (from, to, type and muc#user in presence)
	BufferPtr erase[MAX_ERASE_CHUNKS];
} IncomingPacket;

typedef BOOL(*SendProc)(void *);

typedef struct {
	SendProc proc;
	void *data;
} SendCallback;

typedef struct RouterChunk {
	IncomingPacket input;
	SendCallback send;
	Buffer hostname;
	Rooms *rooms;
	ACLConfig *acl;
	BuilderPacket output;
} RouterChunk;

int router_process(RouterChunk *);
void router_cleanup(IncomingPacket *);
int router_error(RouterChunk *, XMPPError *);

#endif
