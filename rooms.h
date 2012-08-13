#ifndef ROOMS_H
#define ROOMS_H

#include <pthread.h>
#include <stdio.h>

#include "room.h"
#include "jid.h"

typedef struct {
	int count;
	Room *start, *end;
	pthread_mutex_t sync;
} Rooms;

void rooms_init(Rooms *);
void rooms_destroy(Rooms *);

Room *rooms_create(Rooms *, Jid *);
Room *rooms_find(Rooms *, Jid *);
void rooms_acquire(Room *);
void rooms_release(Room *);

typedef struct RouterChunk RouterChunk;
int rooms_route(RouterChunk *);

BOOL rooms_serialize(Rooms *, FILE *);
BOOL rooms_deserialize(Rooms *, FILE *, int);

#endif
