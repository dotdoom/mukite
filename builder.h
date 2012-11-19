#ifndef BUILDER_H
#define BUILDER_H

#include "xmcomp/common.h"

#include "rooms.h"
#include "room.h"

#define MAX_STATUS_CODES 5

typedef struct {
	int affiliation, role;
	Jid *jid;
	BufferPtr nick;
	int status_codes[MAX_STATUS_CODES];
	int status_codes_count;
} MucAdmNode;

typedef struct {
	char code[4];
	char type[10];
	char name[20];
	char text[200];
} XMPPError;

#define BUILD_IQ_VERSION 1
#define BUILD_IQ_LAST 2
#define BUILD_IQ_TIME 3
#define BUILD_IQ_STATS 4


#define BUILD_IQ_DISCO_INFO 11
#define BUILD_IQ_DISCO_ITEMS 12

#define BUILD_IQ_ROOM 20
#define BUILD_IQ_ROOM_DISCO_INFO 21
#define BUILD_IQ_ROOM_DISCO_ITEMS 22
#define BUILD_IQ_ROOM_AFFILIATIONS 23

typedef struct {
	Buffer from_node, from_host, from_nick;

	Jid to;

	char name, type;

	int iq_type;

	BufferPtr header, user_data;
	union {
		// type='error'
		XMPPError *error;

		// <presence>
		MucAdmNode participant;

		// iq_type = BUILD_IQ_ROOM_DISCO_*
		Room *room;
		
		// iq_type = BUILD_IQ_LAST
		struct {
			double seconds;
		} iq_last;

		// iq_type = BUILD_IQ_STATS
		BufferPtr iq_stats_request;
		
		// iq_type = BUILD_IQ_DISCO_*
		Rooms *rooms;

		// iq_type = BUILD_IQ_ROOM_AFFILIATIONS
		struct {
			int affiliation;
			AffiliationEntry *items;
		} muc_items;
	};
} BuilderPacket;

typedef struct {
	char *data, *data_end, *end;
} BuilderBuffer;

BOOL builder_build(BuilderPacket *packet, BuilderBuffer *buffer);
inline BOOL builder_push_status_code(MucAdmNode *, int);

#endif
