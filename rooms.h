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
void rooms_deinit(Rooms *);

Room *rooms_acquire(Rooms *, Jid *);
void rooms_release(Room *);

void rooms_serialize(Rooms *, FILE *);
int rooms_deserialize(Rooms *, FILE *);

#endif
