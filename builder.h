#ifndef BUILDER_H
#define BUILDER_H

#include "xmcomp/common.h"

#define MAX_STATUS_CODES 4

typedef struct {
	int affiliation, role;
	Jid *jid;
	int status_codes[MAX_STATUS_CODES];
} MucAdmNode;

typedef struct {
	Buffer from_node, from_host, from_nick;

	Jid to;

	char name, type;

	BufferPtr header, user_data;
	union {
		MucAdmNode participant;
		BufferPtr system_data;
	};
} BuilderPacket;

typedef struct {
	char *data, *data_end, *end;
} BuilderBuffer;

BOOL build_packet(BuilderPacket *packet, BuilderBuffer *buffer);

#endif
