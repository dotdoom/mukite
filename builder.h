#ifndef BUILDER_H
#define BUILDER_H

#include "xmcomp/common.h"

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

typedef struct {
	Buffer from_node, from_host, from_nick;

	Jid to;

	char name, type;

	int iq_type;

	BufferPtr header, user_data;
	union {
		MucAdmNode participant;
		XMPPError *error;
		Room *room;
		struct {
			long long seconds;
		} iq_last;
		struct {
			char tzo[10];
			char utc[20];
		} iq_time;
	};
} BuilderPacket;

typedef struct {
	char *data, *data_end, *end;
} BuilderBuffer;

BOOL builder_build(BuilderPacket *packet, BuilderBuffer *buffer);
inline BOOL builder_push_status_code(MucAdmNode *, int);

#endif
