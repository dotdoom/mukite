#ifndef ROOM_H
#define ROOM_H

#include <stdio.h>
#include <pthread.h>

#include "xmcomp/buffer.h"

#include "jid.h"

#define ROLE_NONE 0
#define ROLE_VISITOR 1
#define ROLE_PARTICIPANT 2
#define ROLE_MODERATOR 3

extern const char** affiliation_names;
extern const int* affiliation_name_sizes;

#define AFFIL_NONE -1
#define AFFIL_OUTCAST 0
#define AFFIL_MEMBER 1
#define AFFIL_ADMIN 2
#define AFFIL_OWNER 3

extern const char* role_names[];
extern const int role_name_sizes[];

#define USER_STRING_OPTION_LIMIT 4096
#define PARTICIPANTS_LIMIT 1024
#define AFFILIATION_LIST_LIMIT 4096
#define REASONABLE_RAW_LIMIT (1 << 20)
#define HISTORY_ITEMS_COUNT_LIMIT 100

typedef struct HistoryEntry {
	BufferPtr nick, header, inner;
	time_t delay;
	struct HistoryEntry *next;
} HistoryEntry;

typedef struct AffiliationEntry {
	Jid jid;
	Buffer reason;
	struct AffiliationEntry *next;
} AffiliationEntry;

typedef struct ParticipantEntry {
	Jid jid;
	Buffer nick;
	int affiliation, role;
	BufferPtr presence;
	time_t last_message_time;
	struct ParticipantEntry *prev, *next;
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
#define MUC_FLAG_VISITORSPM (1 << 12)

#define MUC_FLAGS_DEFAULT (MUC_FLAG_ALLOWPM | \
	MUC_FLAG_CHANGESUBJECT | \
	MUC_FLAG_MODERATEDROOM | \
	MUC_FLAG_PUBLICROOM | \
	MUC_FLAG_PUBLICPARTICIPANTS | \
	MUC_FLAG_IQ_PROXY | \
	MUC_FLAG_SEMIANONYMOUS | \
	MUC_FLAG_INVITES)

typedef struct Room {
	Buffer node,
		title,
		description,
		subject,
		password;

	int flags;
	int default_role;
	int max_participants;
	long _unused[7];

	ParticipantEntry *participants;
	int participants_count;

	AffiliationEntry *affiliations[4];

	HistoryEntry *history;

	struct Room *prev, *next;
	pthread_mutex_t sync;
} Room;

void room_init(Room *, BufferPtr *);
void room_destroy(Room *);

void room_lock(Room *);
void room_unlock(Room *);

int room_role_for_affiliation(Room *, int);

ParticipantEntry *room_join(Room *room, Jid *jid, BufferPtr *nick, int affiliation);
void room_leave(Room *room, ParticipantEntry *participant);

ParticipantEntry *room_participant_by_nick(Room *room, BufferPtr *nick);
ParticipantEntry *room_participant_by_jid(Room *room, Jid *jid);

struct RouterChunk;
void room_route(Room *, struct RouterChunk *);

BOOL room_serialize(Room *, FILE *);
BOOL room_deserialize(Room *, FILE *);

#endif
