#ifndef ROOM_H
#define ROOM_H

#include <stdio.h>
#include <pthread.h>

#include "xmcomp/buffer.h"

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

#define USER_STRING_OPTION_LIMIT 4096
#define PARTICIPANTS_LIMIT 1024
#define AFFILIATION_LIST_LIMIT 4096

typedef struct AffiliationEntry_t {
	Jid jid;
	Buffer reason;
	struct AffiliationEntry_t *next;
} AffiliationEntry;

typedef struct ParticipantEntry_t {
	Jid jid;
	Buffer nick;
	int affiliation, role;
	struct ParticipantEntry_t *prev, *next;
} ParticipantEntry;

#define MUC_FLAG_ALLOWPM 1
#define MUC_FLAG_CHANGESUBJECT (1 << 1)
#define MUC_FLAG_ENABLELOGGING (1 << 2)
#define MUC_FLAG_MEMBERSONLY (1 << 3)
#define MUC_FLAG_MODERATEDROOM (1 << 4)
#define MUC_FLAG_PASSWORDPROTECTEDROOM (1 << 5)
#define MUC_FLAG_PERSISTENTROOM (1 << 6)
#define MUC_FLAG_PUBLICROOM (1 << 7)
#define MUC_FLAG_PUBLICPARTICIPANTS (1 << 8)
#define MUC_FLAG_SEMIANONYMOUS (1 << 9)
#define MUC_FLAG_IQ_PROXY (1 << 10)
#define MUC_FLAG_INVITES (1 << 11)

#define MUC_FLAGS_DEFAULT (MUC_FLAG_ALLOWPM | \
	MUC_FLAG_CHANGESUBJECT | \
	MUC_FLAG_MODERATEDROOM | \
	MUC_FLAG_PUBLICROOM | \
	MUC_FLAG_PUBLICPARTICIPANTS | \
	MUC_FLAG_IQ_PROXY | \
	MUC_FLAG_INVITES)

typedef struct Room_t {
	Buffer node,
		title,
		description,
		subject,
		password;

	int flags;
	int default_role;
	int max_participants;
	int _unused[16];

	ParticipantEntry
		*participants;

	AffiliationEntry
		*owners,
		*admins,
		*members,
		*outcasts;

	struct Room_t *prev, *next;
	pthread_mutex_t sync;
} Room;

void room_init(Room *room, BufferPtr *node);
void room_destroy(Room *room);

void room_lock(Room *room);
void room_unlock(Room *room);

ParticipantEntry *room_join(Room *room, Jid *jid, BufferPtr *nick);
void room_leave(Room *room, ParticipantEntry *participant);

ParticipantEntry *room_participant_by_nick(Room *room, BufferPtr *nick);
ParticipantEntry *room_participant_by_jid(Room *room, Jid *jid);

void room_moderate(Room *room, ParticipantEntry *user, int affiliation, int role);

BOOL room_serialize(Room *, FILE *);
BOOL room_deserialize(Room *, FILE *);

#endif
