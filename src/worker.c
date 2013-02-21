#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "xmcomp/src/ringbuffer.h"
#include "xmcomp/src/common.h"
#include "xmcomp/src/logger.h"
#include "xmcomp/src/xmlfsm.h"
#include "xmcomp/src/sighelper.h"

#include "builder.h"
#include "config.h"
#include "component.h"

#include "worker.h"

static pthread_key_t builder_buffer_key;
void worker_establish_local_storage() {
	pthread_key_create(&builder_buffer_key, 0);
}

typedef struct {
	BuilderBuffer builder_buffer;
	RingBuffer *ringbuffer;
	BufferPtr hostname;
} WorkerThreadStorage;

BOOL worker_send(BuilderPacket *packet) {
	WorkerThreadStorage *storage = pthread_getspecific(builder_buffer_key);

	if (!storage) {
		LERROR("[BUG] packet_send called from outside the worker thread - no thread-local storage here, dropping");
		return FALSE;
	}
	storage->builder_buffer.data_end = storage->builder_buffer.data;
	packet->from_host = storage->hostname;
	if (builder_build(packet, &storage->builder_buffer)) {
		ringbuffer_write(storage->ringbuffer, storage->builder_buffer.data,
				storage->builder_buffer.data_end - storage->builder_buffer.data);
		return TRUE;
	} else {
		LERROR("output buffer (%d bytes) is not large enough to hold a stanza - dropped",
				BPT_SIZE(&storage->builder_buffer));
		LDEBUG("bytes in a buffer: '%.*s'",
				(int)(storage->builder_buffer.data_end - storage->builder_buffer.data),
				storage->builder_buffer.data);
		return FALSE;
	}
}

void *worker_thread_entry(void *void_worker_config) {
	WorkerConfig *worker_config = (WorkerConfig *)void_worker_config;
	StanzaQueue *queue = worker_config->queue;
	int allocated_buffer_size = worker_config->builder_buffer_size;

	StanzaEntry *stanza_entry;
	BufferPtr stanza_entry_buffer;
	IncomingPacket ingress;
	WorkerThreadStorage storage;

	storage.builder_buffer.data = malloc(allocated_buffer_size);
	storage.builder_buffer.end = storage.builder_buffer.data + allocated_buffer_size;
	storage.ringbuffer = worker_config->ringbuffer;
	storage.hostname = worker_config->hostname;
	pthread_setspecific(builder_buffer_key, &storage);

	LINFO("started");
	sighelper_sigblockall(0);

	while (worker_config->enabled) {
		if (allocated_buffer_size != worker_config->builder_buffer_size) {
			storage.builder_buffer.data = realloc(storage.builder_buffer.data,
					allocated_buffer_size = worker_config->builder_buffer_size);
			storage.builder_buffer.end = storage.builder_buffer.data + allocated_buffer_size;
		}

		stanza_entry = queue_pop_data(queue);
		if (stanza_entry->buffer && stanza_entry->data_size) {
			stanza_entry_buffer.data = stanza_entry->buffer;
			stanza_entry_buffer.end = stanza_entry->buffer + stanza_entry->data_size;

			if (packet_parse(&ingress, &stanza_entry_buffer)) {
				if (BPT_NULL(&ingress.proxy_to.node)) {
					component_process(&ingress);
				} else {
					rooms_process(worker_config->rooms, &ingress, worker_config->acl);
				}
			}
		}

		queue_push_free(queue, stanza_entry);
	}

	free(storage.builder_buffer.data);

	LINFO("terminated");
	pthread_exit(0);
}

BOOL worker_bounce(IncomingPacket *ingress, XMPPError *error, Buffer *node) {
	BuilderPacket egress = {};

	packet_cleanup(ingress, PACKET_CLEANUP_TO | PACKET_CLEANUP_SWITCH_FROM_TO);

	egress.name = ingress->name;
	egress.type = STANZA_ERROR;
	if (node) {
		egress.from_node = *node;
	}
	egress.header = ingress->header;
	egress.sys_data.error = error;
	egress.user_data = ingress->inner;
	return worker_send(&egress);
}
