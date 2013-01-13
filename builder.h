#ifndef BUILDER_H
#define BUILDER_H

#include <sys/utsname.h>

#include "xmcomp/common.h"
#include "xmcomp/queue.h"
#include "xmcomp/ringbuffer.h"

#include "rooms.h"
#include "room.h"

typedef struct {
	int affiliation, role;
	Jid jid;
	BufferPtr nick;
	BufferPtr reason_node;
} MucAdminItem;

typedef struct {
	char code[4];
	char type[10];
	char name[25];
	char text[200];
} XMPPError;

#define MAX_STATUS_CODES 5

typedef struct {
	int codes[MAX_STATUS_CODES];
	int size;
} StatusCodes;

#define BUILD_IQ_VERSION 1
#define BUILD_IQ_LAST 2
#define BUILD_IQ_TIME 3
#define BUILD_IQ_STATS 4
#define BUILD_IQ_EMPTY 5
#define BUILD_IQ_VCARD 6

#define BUILD_IQ_DISCO_INFO 11
#define BUILD_IQ_DISCO_ITEMS 12

#define BUILD_IQ_ROOM 20
#define BUILD_IQ_ROOM_DISCO_INFO 21
#define BUILD_IQ_ROOM_DISCO_ITEMS 22
#define BUILD_IQ_ROOM_AFFILIATIONS 23
#define BUILD_IQ_ROOM_CONFIG 24

#define STANZA_IQ 1
#define STANZA_IQ_GET 1
#define STANZA_IQ_SET 2
#define STANZA_IQ_RESULT 3

#define STANZA_MESSAGE 2
#define STANZA_MESSAGE_CHAT 1
#define STANZA_MESSAGE_GROUPCHAT 2

#define STANZA_PRESENCE 3
#define STANZA_PRESENCE_UNAVAILABLE 1

#define STANZA_ERROR 100

typedef struct {
	BufferPtr from_node, from_nick;
	Jid to;
	char name, type;

	int iq_type;
	time_t delay;

	BufferPtr header, user_data;
	union {
		// type='error'
		XMPPError *error;

		// <presence>
		struct MucUserNode {
			MucAdminItem item;
			StatusCodes status_codes;
			BufferPtr resume;
		} presence;

		// iq_type = BUILD_IQ_ROOM_DISCO_*, BUILD_IQ_ROOM_CONFIG
		Room *room;

		// iq_type = BUILD_IQ_ROOM_AFFILIATIONS
		struct {
			int affiliation;
			AffiliationEntry *items;
		} muc_items;
	} sys_data;
} BuilderPacket;

typedef struct {
	char *data, *data_end, *end;
} BuilderBuffer;

BOOL builder_build(BuilderPacket *packet, BuilderBuffer *buffer);
inline BOOL builder_push_status_code(StatusCodes *, int);

#endif
