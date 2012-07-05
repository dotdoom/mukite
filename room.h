#ifndef ROOM_H
#define ROOM_H

#include <stdio.h>
#include <pthread.h>

#include "jid.h"

#define ROLE_UNCHANGED -9
#define ROLE_VISITOR 0
#define ROLE_PARTICIPANT 1
#define ROLE_MODERATOR 2

#define AFFIL_UNCHANGED -9
#define AFFIL_OUTCAST -1
#define AFFIL_NONE 0
#define AFFIL_MEMBER 1
#define AFFIL_ADMIN 2
#define AFFIL_OWNER 3

typedef struct AffiliationEntry_t {
	Jid jid;
	char *reason;
	struct AffiliationEntry_t *next;
} AffiliationEntry;

typedef struct ParticipantEntry_t {
	Jid jid;
	Buffer nick;
	int affiliation, role;
	Buffer cached_presence;
	struct ParticipantEntry_t *prev, *next;
} ParticipantEntry;

#define MUC_FLAG_SEMIANONYMOUS 1

typedef struct Room_t {
	Buffer node, name, subject;

	/* ... some flags ... */
	int default_role;
	int flags;

	ParticipantEntry
		*participants;

	AffiliationEntry
		*owners,
		*admins,
		*members,
		*outcasts;

	char affiliation_role_cache_buffer[100];

	struct Room_t *prev, *next;
	pthread_mutex_t sync;
} Room;

void room_init(Room *room, BufferPtr *node);
void room_deinit(Room *room);

void room_lock(Room *room);
void room_unlock(Room *room);

ParticipantEntry *room_join(Room *room, Jid *jid, BufferPtr *nick);
void room_leave(Room *room, ParticipantEntry *participant);

ParticipantEntry *room_participant_by_nick(Room *room, BufferPtr *nick);
ParticipantEntry *room_participant_by_jid(Room *room, Jid *jid);

void room_moderate(Room *room, ParticipantEntry *user, int affiliation, int role);

void room_serialize(Room *room, FILE *output);
int room_deserialize(Room *room, FILE *input);

#endif
