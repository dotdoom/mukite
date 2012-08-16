#include <string.h>

#include "xmcomp/logger.h"

#include "jid.h"

#include "builder.h"

#define BUF_PUSH(data, size) \
	{ \
		chunk_size = (size); \
		if (buffer->data_end + chunk_size > buffer->end) { \
			return FALSE; \
		} \
		memcpy(buffer->data_end, (data), chunk_size); \
		buffer->data_end += chunk_size; \
	}

#define BUF_PUSH_BUF(bptr) \
	BUF_PUSH(bptr.data, bptr.size)

#define BUF_PUSH_STR(str) \
	BUF_PUSH((str), strlen(str))

#define BUF_PUSH_BPT(bptr) \
	BUF_PUSH(bptr.data, BPT_SIZE(&bptr))

#define BUF_PUSH_IFBPT(bptr) \
	{ if (bptr.data) { BUF_PUSH_BPT(bptr) } }

#define BUF_PUSH_LITERAL(data) \
	BUF_PUSH(data, sizeof(data)-1)

static const char* affiliations[] = {
	"outcast",
	"none",
	"member",
	"admin",
	"owner"
};
static const int affiliation_sizes[] = {
	7,
	4,
	6,
	5,
	5
};

static const char* roles[] = {
	"visitor",
	"participant",
	"moderator"
};
static const int role_sizes[] = {
	7,
	11,
	9
};

BOOL build_presence_mucadm(MucAdmNode *node, BuilderBuffer *buffer) {
	int i, code, chunk_size;
	char code_str[3];

	BUF_PUSH_LITERAL("<x xmlns='http://jabber.org/protocol/muc#user'><item affiliation='");
	BUF_PUSH(affiliations[node->affiliation+1], affiliation_sizes[node->affiliation+1]);
	BUF_PUSH_LITERAL("' role='");
	BUF_PUSH(roles[node->role], role_sizes[node->role]);
	if (node->jid) {
		BUF_PUSH_LITERAL("' jid='");
		BUF_PUSH(JID_STR(node->jid), JID_LEN(node->jid));
	}
	if (node->nick.data) {
		BUF_PUSH_LITERAL("' nick='");
		BUF_PUSH_BPT(node->nick);
	}

	for (i = 0; i < node->status_codes_count && (code = node->status_codes[i]); ++i) {
		BUF_PUSH_LITERAL("'/><status code='");
		code_str[2] = code % 10 + '0';
		code /= 10;
		code_str[1] = code % 10 + '0';
		code /= 10;
		code_str[0] = code + '0';
		BUF_PUSH(code_str, 3);
	}

	BUF_PUSH_LITERAL("'/></x>");
	return TRUE;
}

BOOL build_error(XMPPError *error, BuilderBuffer *buffer) {
	int chunk_size;

	BUF_PUSH_LITERAL("<error code='");
	BUF_PUSH_STR(error->code);
	BUF_PUSH_LITERAL("' type='");
	BUF_PUSH_STR(error->type);
	BUF_PUSH_LITERAL("'><");
	BUF_PUSH_STR(error->name);
	BUF_PUSH_LITERAL(" xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/><text>");
	BUF_PUSH_STR(error->text);
	BUF_PUSH_LITERAL("</text></error>");

	return TRUE;
}

BOOL builder_build(BuilderPacket *packet, BuilderBuffer *buffer) {
	int chunk_size;

	LDEBUG("building packet: started");

	BUF_PUSH_BPT(packet->header);
	BUF_PUSH_LITERAL(" from='");
	if (packet->from_node.data) {
		BUF_PUSH_BUF(packet->from_node);
		BUF_PUSH_LITERAL("@");
	}
	BUF_PUSH_BUF(packet->from_host);
	if (packet->from_nick.data) {
		BUF_PUSH_LITERAL("/");
		BUF_PUSH_BUF(packet->from_nick);
	}
	BUF_PUSH_LITERAL("' to='");
	BUF_PUSH(JID_STR(&packet->to), JID_LEN(&packet->to));

	if (packet->type) {
		BUF_PUSH_LITERAL("' type='");
		switch (packet->type) {
			case 'g':
				if (packet->name == 'm') {
					BUF_PUSH_LITERAL("groupchat");
				} else {
					// iq
					BUF_PUSH_LITERAL("get");
				}
				break;
			case 'u':
				BUF_PUSH_LITERAL("unavailable");
				break;
			case 'c':
				BUF_PUSH_LITERAL("chat");
				break;
			case 'r':
				BUF_PUSH_LITERAL("result");
				break;
			case 'e':
				BUF_PUSH_LITERAL("error");
				break;
			case 's':
				BUF_PUSH_LITERAL("set");
				break;
		}
	}

	if (!packet->header.data &&
			!packet->user_data.data &&
			(packet->name == 'p'/* || !packet->system_data.data*/)) {
		BUF_PUSH_LITERAL("'/>");
	} else {
		BUF_PUSH_LITERAL("'>");

		BUF_PUSH_IFBPT(packet->user_data);
		if (packet->type == 'e') {
			if (!build_error(packet->error, buffer)) {
				return FALSE;
			}
		} else {
			if (packet->name == 'p') {
				if (!build_presence_mucadm(&packet->participant, buffer)) {
					return FALSE;
				}
			} else {
				//BUF_PUSH_IFBPT(packet->system_data);
			}
		}
		switch (packet->name) {
			case 'm':
				BUF_PUSH_LITERAL("</message>");
				break;
			case 'p':
				BUF_PUSH_LITERAL("</presence>");
				break;
			case 'i':
				BUF_PUSH_LITERAL("</iq>");
				break;
		}
	}

	return TRUE;
}

BOOL builder_push_status_code(MucAdmNode *participant, int code) {
	if (participant->status_codes_count >= MAX_STATUS_CODES) {
		return FALSE;
	}
	participant->status_codes[participant->status_codes_count++] = code;
	return TRUE;
}
