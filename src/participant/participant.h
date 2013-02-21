#ifndef PARTICIPANT_H
#define PARTICIPANT_H

#include <time.h>

#include "jid.h"

#define MAX_PRESENCE_SIZE 65000

typedef struct Participant {
	Jid jid;
	BufferPtr nick;
	int affiliation, role;
	BufferPtr presence;
	time_t last_message_time, last_presence_time;

	struct {
		// This flag is used to avoid duplicates when building affected list
		BOOL included;
		BufferPtr reason_node;
		struct Participant *next;
	} affected_list;

	struct Participant *prev, *next;
} Participant;

Participant *participant_init(Participant *, Jid *);
Participant *participant_destroy(Participant *);

BOOL participant_set_nick(Participant *, BufferPtr *);
BOOL Participant_set_presence(Participant *, BufferPtr *);

BOOL participant_serialize(Participant *, FILE *);
BOOL participant_deserialize(Participant *, FILE *);

void participant_set_affected(Participant **first_affected, Participant **current_affected,
		Participant *new_affected, BufferPtr *reason_node);

#endif
