#ifndef BUILDER_H
#define BUILDER_H

#include <sys/utsname.h>

#include "xmcomp/src/common.h"
#include "xmcomp/src/queue.h"
#include "xmcomp/src/ringbuffer.h"

#include "room/rooms.h"

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

// http://xmpp.org/extensions/xep-0045.html#registrar-statuscodes-init
#define STATUS_NON_ANONYMOUS 1
#define STATUS_SELF_PRESENCE (1 << 1)
#define STATUS_LOGGING_ENABLED (1 << 2)
#define STATUS_ROOM_CREATED (1 << 3)
#define STATUS_NICKNAME_ENFORCED (1 << 4)
#define STATUS_BANNED (1 << 5)
#define STATUS_NICKNAME_CHANGED (1 << 6)
#define STATUS_KICKED (1 << 7)
#define STATUS_NONMEMBER_REMOVED (1 << 8)

typedef struct {
	BufferPtr from_nick, from_host;
	Buffer from_node;
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
			int status_codes;
			BufferPtr resume;
		} presence;

		// iq_type = BUILD_IQ_ROOM_DISCO_*, BUILD_IQ_ROOM_CONFIG
		Room *room;

		// iq_type = BUILD_IQ_DISCO_ITEMS
		Rooms *rooms;

		// iq_type = BUILD_IQ_ROOM_AFFILIATIONS
		struct {
			int affiliation;
			Affiliation *items;
		} muc_items;

		// iq_type = BUILD_IQ_VERSION
		struct utsname uname;

		// iq_type = BUILD_IQ_STATS
		struct {
			RingBufferStats *ringbuffer;
			StanzaQueueStats *queue;
			Rooms *rooms;
		} stats;
	} sys_data;
} BuilderPacket;

typedef struct {
	char *data, *data_end, *end;
} BuilderBuffer;

BOOL builder_build(BuilderPacket *packet, BuilderBuffer *buffer);

#endif
