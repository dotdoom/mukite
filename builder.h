#ifndef BUILDER_H
#define BUILDER_H

#include "xmcomp/common.h"

#define MAX_STATUS_CODES 5

typedef struct {
	int affiliation, role;
	Jid *jid;
	int status_codes[MAX_STATUS_CODES];
	int status_codes_count;
} MucAdmNode;

typedef struct {
	char code[4];
	char type[10];
	char name[20];
	char text[200];
} XMPPError;

typedef struct {
	Buffer from_node, from_host, from_nick;

	Jid to;

	char name, type;

	BufferPtr header, user_data;
	union {
		MucAdmNode participant;
		XMPPError *error;
	};
} BuilderPacket;

typedef struct {
	char *data, *data_end, *end;
} BuilderBuffer;

BOOL builder_build(BuilderPacket *packet, BuilderBuffer *buffer);
inline BOOL builder_push_status_code(MucAdmNode *, int);

#endif
