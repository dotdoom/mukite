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

void rooms_init(Rooms *rooms);
void rooms_deinit(Rooms *rooms);

Room *rooms_acquire(Rooms *rooms, Jid *jid);
void rooms_release(Room *room);

void rooms_serialize(Rooms *rooms, FILE *output);
int rooms_deserialize(Rooms *rooms, FILE *input);

#endif
