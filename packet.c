#include "xmcomp/common.h"
#include "xmcomp/buffer.h"

#include "packet.h"
/*
inline BOOL set_type(BufferPtr *value, IncomingPacket *packet) {
	BOOL value_null = BPT_NULL(value);
	if (BPT_EQ_LIT("error", value)) {
		packet->type = STANZA_ERROR;
		return TRUE;
	}

	switch (packet->name) {
		case STANZA_MESSAGE:
			if (!value_null) {
				if (BPT_EQ_LIT("chat", value)) {
					packet->type = STANZA_MESSAGE_CHAT;
					return TRUE;
				} else if (BPT_EQ_LIT("groupchat", value)) {
					packet->type = STANZA_MESSAGE_GROUPCHAT;
					return TRUE;
				}
			}
			break;
		case 'p':
			if (value_null) {
				return TRUE;
			} else if (BPT_EQ_LIT("unavailable", value)) {
				packet->type = STANZA_PRESENCE_UNAVAILABLE;
				return TRUE;
			}
			break;
		case 'i':
			if (!value_null) {
				if (BPT_EQ_LIT("get", value)) {
					packet->type = STANZA_IQ_GET;
					return TRUE;
				} else if (BPT_EQ_LIT("set", value)) {
					packet->type = STANZA_IQ_SET;
					return TRUE;
				} else if (BPT_EQ_LIT("result", value)) {
					packet->type = STANZA_IQ_RESULT;
					return TRUE;
				}
			}
			break;
	}

	return FALSE;
}

BOOL packet_parse(IncomingPacket *packet, BufferPtr *buffer) {
	XmlAttr attr;
	Buffer stanza_name;
	int xmlattr_state;

	memset(packet, 0, sizeof(*packet));

	// Parse stanza name
	if (xmlfsm_node_name(buffer, &stanza_name) == XMLPARSE_FAILURE) {
		LWARN("dropping: stanza name parsing failure");
		return FALSE;
	}
	if (BUF_EQ_LIT("message", &stanza_name)) {
		packet->name = STANZA_MESSAGE;
	} else if (BUF_EQ_LIT("presence", &stanza_name)) {
		packet->name = STANZA_PRESENCE;
	} else if (BUF_EQ_LIT("iq", &stanza_name)) {
		packet->name = STANZA_IQ;
	} else {
		LWARN("dropping: unknown stanza name '%.*s'", stanza_name.size, stanza_name.data);
		return FALSE;
	}

	packet->header.data = packet->header.end = buffer->data;

	// Parse attrs
	while ((xmlattr_state = xmlfsm_next_attr(buffer, &attr)) == XMLPARSE_SUCCESS) {
		packet->header.end = buffer->data;
		if (BPT_EQ_LIT("from", &attr.name)) {
			if (!jid_struct(&attr.value, &packet->real_from)) {
				LWARN("'from' jid malformed: '%.*s'",
						BPT_SIZE(&attr.value), attr.value.data);
				return FALSE;
			}
			packet->from_attr = attr;
		} else if (BPT_EQ_LIT("to", &attr.name)) {
			if (!jid_struct(&attr.value, &packet->proxy_to)) {
				LWARN("'to' jid malformed: '%.*s'",
						BPT_SIZE(&attr.value), attr.value.data);
				return FALSE;
			}
			packet->to_attr = attr;
		} else if (BPT_EQ_LIT("type", &attr.name)) {
			if (!set_type(&attr.value, packet)) {
				return FALSE;
			}
			packet->type_attr = attr;
		} else if (BPT_EQ_LIT("id", &attr.name)) {
			packet->id_attr = attr;
		}
	}
	if (xmlattr_state == XMLPARSE_FAILURE) {
		LWARN("dropping: attr parsing failure");
		return FALSE;*/
