#ifndef ROOMS_H
#define ROOMS_H

#include <pthread.h>
#include <stdio.h>

#include "room.h"
#include "jid.h"
#include "acl.h"
#include "packet.h"

#define MAX_REGISTERED_NICKS 10240

typedef struct RegisteredNick {
	Jid jid;
	Buffer nick;
	struct RegisteredNick *next;
} RegisteredNick;

typedef struct {
	int size;
	Room *first, *last;
	RegisteredNick *registered_nicks;
	pthread_rwlock_t sync;
} Rooms;

void rooms_init(Rooms *);
void rooms_destroy(Rooms *);

Room *rooms_create(Rooms *, Jid *);

void rooms_process(IncomingPacket *);

BOOL rooms_serialize(Rooms *, FILE *);
BOOL rooms_deserialize(Rooms *, FILE *, int);

#endif
