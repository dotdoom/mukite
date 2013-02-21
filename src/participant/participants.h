#ifndef PARTICIPANTS_H
#define PARTICIPANTS_H

#include "dls_list.h"

#include "participant.h"

typedef struct {
	DLS_DECLARE(Participant);
} ParticipantsList;

Participant *participants_find_by_nick(ParticipantsList *, BufferPtr *);
Participant *participants_find_by_jid(ParticipantsList *, Jid *);

BOOL participants_serialize(ParticipantsList *, FILE *);
BOOL participants_deserialize(ParticipantsList *, FILE *);

#endif
