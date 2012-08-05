#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "xmcomp/cbuffer.h"
#include "xmcomp/common.h"
#include "xmcomp/logger.h"
#include "xmcomp/xmlfsm.h"
#include "xmcomp/sighelper.h"

#include "router.h"
#include "room.h"
#include "rooms.h"
#include "router.h"
#include "builder.h"
#include "config.h"

#include "parser.h"

inline BOOL set_type(BufferPtr *value, IncomingPacket *packet) {
	int value_size = BPT_SIZE(value);
	if (BPT_EQ_LIT("error", value)) {
		// TODO(artem): route errors, drop participant from the room in some cases
		LDEBUG("not handling yet: stanza type error");
		return FALSE;
	}

	switch (packet->name) {
		case 'm':
			if (!value_size ||
					(!BPT_EQ_LIT("chat", value) &&
					 !BPT_EQ_LIT("groupchat", value))) {
				LDEBUG("dropping: unknown message type '%.*s'",
						value_size, value->data);
				return FALSE;
			}
			break;
		case 'p':
			if (value_size && !BPT_EQ_LIT("unavailable", value)) {
				LDEBUG("dropping: unknown presence type '%.*s'",
						value_size, value->data);
				return FALSE;
			}
			break;
		case 'i':
			if (!value_size ||
					(!BPT_EQ_LIT("get", value) &&
					 !BPT_EQ_LIT("set", value) &&
					 !BPT_EQ_LIT("result", value))) {
				LDEBUG("dropping: unknown iq type '%.*s'",
						value_size, value->data);
				return FALSE;
			}
			break;
	}

	packet->type = value_size ? *value->data : 0;
	return TRUE;
}

BOOL parse_incoming_packet(BufferPtr *buffer, IncomingPacket *packet) {
	XmlAttr attr;
	Buffer stanza_name;
	char erase;
	int retval, erase_index = 0;

	memset(packet, 0, sizeof(*packet));
	packet->header.data = buffer->data;

	// Parse stanza name
	if (xmlfsm_node_name(buffer, &stanza_name) == XMLPARSE_FAULT) {
		LWARN("dropping: stanza name parsing failure");
		return FALSE;
	}
	LDEBUG("got stanza name '%.*s'", stanza_name.size, stanza_name.data);
	if (
			!B_EQ_LIT("message", &stanza_name) &&
			!B_EQ_LIT("presence", &stanza_name) &&
			!B_EQ_LIT("iq", &stanza_name)) {
		LWARN("dropping: unknown stanza name '%.*s'", stanza_name.size, stanza_name.data);
		return FALSE;
	}
	packet->name = *stanza_name.data;
	packet->header.end = buffer->data;

	// Parse attrs
	while ((retval = xmlfsm_get_attr(buffer, &attr)) == XMLPARSE_SUCCESS) {
		packet->header.end = buffer->data;
		LDEBUG("got attr '%.*s'='%.*s'",
				BPT_SIZE(&attr.name), attr.name.data,
				BPT_SIZE(&attr.value), attr.value.data);
		erase = 1;
		if (BPT_EQ_LIT("from", &attr.name)) {
			if (jid_struct(&attr.value, &packet->real_from)) {
				LWARN("'from' jid malformed: '%.*s'",
						BPT_SIZE(&attr.value), attr.value.data);
				return FALSE;
			}
		} else if (BPT_EQ_LIT("to", &attr.name)) {
			if (jid_struct(&attr.value, &packet->proxy_to)) {
				LWARN("'to' jid malformed: '%.*s'",
						BPT_SIZE(&attr.value), attr.value.data);
				return FALSE;
			}
		} else if (BPT_EQ_LIT("type", &attr.name)) {
			if (!set_type(&attr.value, packet)) {
				return FALSE;
			}
			//erase = packet->name == 'i';
		} else {
			erase = 0;
		}

		if (erase) {
			packet->erase[erase_index].data = attr.name.data;
			packet->erase[erase_index].end = attr.value.end+1;
			++erase_index;
		}
	}
	if (retval == XMLPARSE_FAULT) {
		LWARN("dropping: attr parsing failure");
		return FALSE;
	}
	if (JID_EMPTY(&packet->real_from) || JID_EMPTY(&packet->proxy_to)) {
		LWARN("dropping: not routable");
		return FALSE;
	}
	if (packet->name == 'm' &&
			((packet->proxy_to.resource.data == 0) == (packet->type == 'c'))) {
		// The nickname (resource) is specified and type is not c(hat) - thus g(roupchat)
		LDEBUG("dropping: wrong message type");
		return FALSE;
	}

	// Set inner data
	if (retval == XMLNODE_EMPTY) {
		packet->inner.data = 0;
		packet->inner.end = 0;
	} else {
		packet->inner.data = buffer->data;
		packet->inner.end = buffer->end - stanza_name.size - 3;
	}

	return TRUE;
}

typedef struct {
	BuilderBuffer buffer;
	CBuffer *cbuffer;
} LocalBufferStorage;

int send_packet(void *void_local_buffer_storage, BuilderPacket *packet) {
	int retval;
	LocalBufferStorage *lbs = (LocalBufferStorage *)void_local_buffer_storage;
	if (build_packet(packet, &lbs->buffer)) {
		cbuffer_write(lbs->cbuffer, lbs->buffer.data, lbs->buffer.data_end - lbs->buffer.data);
		retval = 1;
	} else {
		LERROR("output buffer (%d bytes) is not large enough to hold a stanza", BPT_SIZE(&lbs->buffer));
		retval = 0;
	}
	lbs->buffer.data_end = lbs->buffer.data;
	return retval;
}

void *parser_thread_entry(void *void_parser_config) {
	ParserConfig *parser_config = (ParserConfig *)void_parser_config;
	Config *config = (Config *)parser_config->global_config;
	Rooms *rooms = &config->rooms;
	StanzaQueue *queue = &config->reader_thread.queue;
	int allocated_buffer_size = config->parser.buffer;

	StanzaEntry *stanza_entry;
	IncomingPacket incoming_packet;
	BufferPtr stanza_entry_buffer;
	LocalBufferStorage lbs;
	SendCallback send_callback;
	int receivers;

	lbs.buffer.data = malloc(allocated_buffer_size);
	lbs.buffer.end = lbs.buffer.data + allocated_buffer_size;
	lbs.cbuffer = &config->writer_thread.cbuffer;

	send_callback.proc = send_packet;
	send_callback.data = &lbs;

	LINFO("started");
	sighelper_sigblockall();

	while (parser_config->enabled) {
		if (allocated_buffer_size != config->parser.buffer) {
			lbs.buffer.data = realloc(lbs.buffer.data,
					allocated_buffer_size = config->parser.buffer);
			lbs.buffer.end = lbs.buffer.data + allocated_buffer_size;
		}

		stanza_entry = queue_pop_data(queue);
		if (stanza_entry->buffer) {
			stanza_entry_buffer.data = stanza_entry->buffer;
			stanza_entry_buffer.end = stanza_entry->buffer + stanza_entry->data_size;

			if (parse_incoming_packet(&stanza_entry_buffer, &incoming_packet)) {
				if ((incoming_packet.room = rooms_acquire(rooms, &incoming_packet.proxy_to))) {
					lbs.buffer.data_end = lbs.buffer.data;
					receivers = route(&incoming_packet, &send_callback, config->component.hostname);
					LDEBUG("forwarded to %d JIDs", receivers);
					rooms_release(incoming_packet.room);
				}
			}
		}

		queue_push_free(queue, stanza_entry);
	}

	LINFO("terminated");
	pthread_exit(0);
}
