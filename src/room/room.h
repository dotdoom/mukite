#ifndef ROOM_H
#define ROOM_H

#include <stdio.h>
#include <pthread.h>

#include "xmcomp/src/buffer.h"

#include "participant/participants.h"
#include "affiliation/affiliations.h"
#include "history_entry/history_entries.h"

#include "jid.h"
#include "packet.h"
#include "acl.h"

#define ROLE_UNCHANGED -2
#define ROLE_NONE 0
#define ROLE_VISITOR 1
#define ROLE_PARTICIPANT 2
#define ROLE_MODERATOR 3

extern const char* role_names[];
extern const int role_name_sizes[];

#define USER_STRING_OPTION_LIMIT 8192
#define PARTICIPANTS_LIMIT 1024
#define AFFILIATION_LIST_LIMIT 4096
#define REASONABLE_RAW_LIMIT (1 << 20)
#define HISTORY_ITEMS_COUNT_LIMIT 100

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
#define MUC_FLAG_VISITORSPM (1 << 12)
#define MUC_FLAG_VISITORPRESENCE (1 << 13)
#define MUC_FLAG_JUST_CREATED (1 << 14)
#define MUC_FLAG_DESTROYED (1 << 15)
#define MUC_FLAG_MEWCAT (1 << 16)

#define MUC_FLAGS_DEFAULT (MUC_FLAG_ALLOWPM | \
	MUC_FLAG_CHANGESUBJECT | \
	MUC_FLAG_MODERATEDROOM | \
	MUC_FLAG_PUBLICROOM | \
	MUC_FLAG_PUBLICPARTICIPANTS | \
	MUC_FLAG_IQ_PROXY | \
	MUC_FLAG_SEMIANONYMOUS | \
	MUC_FLAG_JUST_CREATED | \
	MUC_FLAG_INVITES)

typedef struct Room {
	Buffer
		node,
		title,
		description,
		password;

	int flags;
	int default_role;
	int _unused[12];

	ParticipantsList participants;
	AffiliationsList affiliations[5];
	HistoryEntriesList history;

	struct {
		BufferPtr node, nick;
	} subject;

	UT_hash_handle hh;
	pthread_mutex_t sync;
} Room;

void room_init(Room *, BufferPtr *);
void room_destroy(Room *);

void room_lock(Room *);
void room_unlock(Room *);

int room_role_for_affiliation(Room *, int);

Participant *room_join(Room *room, Jid *jid, BufferPtr *nick, int affiliation);
void room_leave(Room *room, Participant *participant);

void room_route(Room *, IncomingPacket *, ACLConfig *, int deciseconds_limit);

BOOL room_serialize(Room *, FILE *);
BOOL room_deserialize(Room *, FILE *);

#endif
