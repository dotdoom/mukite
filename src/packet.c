#include <string.h>

#include "xmcomp/src/common.h"
#include "xmcomp/src/buffer.h"
#include "xmcomp/src/logger.h"

#include "builder.h"

#include "packet.h"

void packet_cleanup(IncomingPacket *packet, int mode) {
	if (!BPT_NULL(&packet->inner_nodes.presence.muc)) {
		memset(packet->inner_nodes.presence.muc.data, ' ',
				BPT_SIZE(&packet->inner_nodes.presence.muc));
	}
	if (!BPT_NULL(&packet->inner_nodes.presence.muc_user)) {
		memset(packet->inner_nodes.presence.muc_user.data, ' ',
				BPT_SIZE(&packet->inner_nodes.presence.muc_user));
	}
	if (!BPT_NULL(&packet->type_attr.name)) {
		memset(packet->type_attr.name.data, ' ',
				packet->type_attr.value.end - packet->type_attr.name.data + 1);
	}
	if (mode & PACKET_CLEANUP_ID) {
		memset(packet->id_attr.name.data, ' ',
				packet->id_attr.value.end - packet->id_attr.name.data + 1);
	}
	if (mode & PACKET_CLEANUP_TO) {
		memset(packet->to_attr.name.data, ' ',
				packet->to_attr.value.end - packet->to_attr.name.data + 1);
	}
	if ((mode & PACKET_CLEANUP_SWITCH_FROM_TO) == PACKET_CLEANUP_SWITCH_FROM_TO) {
		memcpy(packet->from_attr.name.data, "  to",
				sizeof("  to")-1);
	} else if (mode & PACKET_CLEANUP_FROM) {
		memset(packet->from_attr.name.data, ' ',
				packet->from_attr.value.end - packet->from_attr.name.data + 1);
	}
}

inline static BOOL parse_presence_inner(IncomingPacket *packet) {
	XmlNodeTraverser nodes = { .buffer = packet->inner };
	XmlAttr attr;

	while (xmlfsm_next_sibling(&nodes)) {
		if (BUF_EQ_LIT("x", &nodes.node_name)) {
			if (!xmlfsm_skipto_attr(&nodes.node, "xmlns", &attr)) {
				continue;
			}

			if (BPT_EQ_LIT("http://jabber.org/protocol/muc#user", &attr.value)) {
				if (BPT_NULL(&packet->inner_nodes.presence.muc_user)) {
					packet->inner_nodes.presence.muc_user.data = nodes.node_start;
					packet->inner_nodes.presence.muc_user.end = nodes.node.end;
				} else {
					LWARN("duplicate x:muc#user node, dropping");
					return FALSE;
				}
			} else if (BPT_EQ_LIT("http://jabber.org/protocol/muc", &attr.value)) {
				if (BPT_NULL(&packet->inner_nodes.presence.muc)) {
					packet->inner_nodes.presence.muc.data = nodes.node_start;
					packet->inner_nodes.presence.muc.end = nodes.node.end;
				} else {
					LWARN("duplicate x:muc node, dropping");
					return FALSE;
				}
			} else {
				continue;
			}
		}
	}

	return TRUE;
}

inline static BOOL set_type(BufferPtr *value, IncomingPacket *packet) {
	BOOL value_null = BPT_NULL(value);
	if (!value_null && BPT_EQ_LIT("error", value)) {
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
		case STANZA_PRESENCE:
			if (value_null) {
				return TRUE;
			} else if (BPT_EQ_LIT("unavailable", value)) {
				packet->type = STANZA_PRESENCE_UNAVAILABLE;
				return TRUE;
			}
			break;
		case STANZA_IQ:
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

	// Set inner data
	if (xmlattr_state == XMLNODE_EMPTY) {
		BPT_INIT(&packet->inner);
	} else {
		packet->inner.data = buffer->data;
		packet->inner.end = buffer->end - stanza_name.size - 3;
	}

	if (JID_EMPTY(&packet->real_from) || JID_EMPTY(&packet->proxy_to)) {
		LWARN("dropping: not routable");
		return FALSE;
	}

	if (packet->name == STANZA_MESSAGE || packet->name == STANZA_PRESENCE) {
		if (BPT_BLANK(&packet->proxy_to.node)) {
			// Messages and presences MUST contain room node name
			LDEBUG("dropping: message/presence without node name");
			return FALSE;
		}
		if (packet->name == STANZA_MESSAGE && BPT_NULL(&packet->proxy_to.resource) &&
				packet->type != STANZA_MESSAGE_GROUPCHAT) {
			LDEBUG("dropping: wrong message type for groupchat");
			return FALSE;
		}
	}

	if (packet->name == STANZA_PRESENCE) {
		if (!parse_presence_inner(packet)) {
			return FALSE;
		}
	}

	LDEBUG("parsed stanza '%.*s'", stanza_name.size, stanza_name.data);

	return TRUE;
}
