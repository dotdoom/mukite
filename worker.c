#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "xmcomp/ringbuffer.h"
#include "xmcomp/common.h"
#include "xmcomp/logger.h"
#include "xmcomp/xmlfsm.h"
#include "xmcomp/sighelper.h"

#include "router.h"
#include "builder.h"
#include "config.h"

#include "worker.h"

inline BOOL set_type(BufferPtr *value, IncomingPacket *packet) {
	int value_size = BPT_SIZE(value);
	if (BPT_EQ_LIT("error", value)) {
		packet->type = 'e';
		return TRUE;
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
	if (xmlfsm_node_name(buffer, &stanza_name) == XMLPARSE_FAILURE) {
		LWARN("dropping: stanza name parsing failure");
		return FALSE;
	}
	if (
			!BUF_EQ_LIT("message", &stanza_name) &&
			!BUF_EQ_LIT("presence", &stanza_name) &&
			!BUF_EQ_LIT("iq", &stanza_name)) {
		LWARN("dropping: unknown stanza name '%.*s'", stanza_name.size, stanza_name.data);
		return FALSE;
	}
	packet->name = *stanza_name.data;
	packet->header.data +=
		(packet->name == 'm' ? 8 :
		 packet->name == 'p' ? 9 : 3);
	packet->header.end = buffer->data;

	// Parse attrs
	while ((retval = xmlfsm_get_attr(buffer, &attr)) == XMLPARSE_SUCCESS) {
		packet->header.end = buffer->data;
		erase = 1;
		if (BPT_EQ_LIT("from", &attr.name)) {
			if (!jid_struct(&attr.value, &packet->real_from)) {
				LWARN("'from' jid malformed: '%.*s'",
						BPT_SIZE(&attr.value), attr.value.data);
				return FALSE;
			}
		} else if (BPT_EQ_LIT("to", &attr.name)) {
			if (!jid_struct(&attr.value, &packet->proxy_to)) {
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
	if (retval == XMLPARSE_FAILURE) {
		LWARN("dropping: attr parsing failure");
		return FALSE;
	}
	if (JID_EMPTY(&packet->real_from) || JID_EMPTY(&packet->proxy_to)) {
		LWARN("dropping: not routable");
		return FALSE;
	}
	if (packet->name == 'm' || packet->name == 'p') {
		if (BPT_BLANK(&packet->proxy_to.node)) {
			// Messages and presences MUST contain room node name
			LDEBUG("dropping: message/presence without node name");
			return FALSE;
		}
		if (packet->name == 'm' && BPT_NULL(&packet->proxy_to.resource) && packet->type != 'g') {
			LDEBUG("dropping: wrong message type for groupchat");
			return FALSE;
		}
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
	RingBuffer *ringbuffer;
	BuilderPacket *packet;
} LocalBufferStorage;

BOOL send_packet(void *void_local_buffer_storage) {
	LocalBufferStorage *lbs = (LocalBufferStorage *)void_local_buffer_storage;
	lbs->buffer.data_end = lbs->buffer.data;
	if (builder_build(lbs->packet, &lbs->buffer)) {
		LDEBUG("writing %d bytes to ringbuffer", (int)(lbs->buffer.data_end - lbs->buffer.data));
		ringbuffer_write(lbs->ringbuffer, lbs->buffer.data, lbs->buffer.data_end - lbs->buffer.data);
		LDEBUG("writing to ringbuffer - complete");
		return TRUE;
	} else {
		LERROR("worker output buffer (%d bytes) is not large enough to hold a stanza - dropped",
				BPT_SIZE(&lbs->buffer));
		return FALSE;
	}
}

void *worker_thread_entry(void *void_worker_config) {
	WorkerConfig *worker_config = (WorkerConfig *)void_worker_config;
	Config *config = (Config *)worker_config->global_config;
	StanzaQueue *queue = &config->reader_thread.queue;
	int allocated_buffer_size = config->worker.buffer;

	StanzaEntry *stanza_entry;
	BufferPtr stanza_entry_buffer;
	LocalBufferStorage lbs;
	RouterChunk router_chunk;

	router_chunk.send.proc = send_packet;
	router_chunk.send.data = &lbs;
	router_chunk.config = config;
	router_chunk.acl = &config->acl_config;
	router_chunk.hostname.data = config->component.hostname;
	router_chunk.hostname.size = strlen(config->component.hostname);

	lbs.buffer.data = malloc(allocated_buffer_size);
	lbs.buffer.end = lbs.buffer.data + allocated_buffer_size;
	lbs.ringbuffer = &config->writer_thread.ringbuffer;
	lbs.packet = &router_chunk.egress;

	LINFO("started");
	sighelper_sigblockall(0);

	while (worker_config->enabled) {
		if (allocated_buffer_size != config->worker.buffer) {
			lbs.buffer.data = realloc(lbs.buffer.data,
					allocated_buffer_size = config->worker.buffer);
			lbs.buffer.end = lbs.buffer.data + allocated_buffer_size;
		}

		stanza_entry = queue_pop_data(queue);
		if (stanza_entry->buffer) {
			stanza_entry_buffer.data = stanza_entry->buffer;
			stanza_entry_buffer.end = stanza_entry->buffer + stanza_entry->data_size;

			if (parse_incoming_packet(&stanza_entry_buffer, &router_chunk.ingress)) {
				lbs.buffer.data_end = lbs.buffer.data;
				router_process(&router_chunk);
			}
		}

		queue_push_free(queue, stanza_entry);
	}

	LINFO("terminated");
	pthread_exit(0);
}
