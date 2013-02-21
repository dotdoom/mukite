#ifndef WORKER_H
#define WORKER_H

#include <pthread.h>

#include "xmcomp/src/common.h"
#include "xmcomp/src/queue.h"
#include "xmcomp/src/ringbuffer.h"

#include "builder.h"
#include "room/rooms.h"

typedef struct {
	BOOL enabled;

	StanzaQueue *queue;
	RingBuffer *ringbuffer;

	int builder_buffer_size;

	Rooms *rooms;
	ACLConfig *acl;
	BufferPtr hostname;

	pthread_t thread;
} WorkerConfig;

void *worker_thread_entry(void *void_worker_config);

void worker_establish_local_storage();
BOOL worker_send(BuilderPacket *);
BOOL worker_bounce(IncomingPacket *, XMPPError *, Buffer *node);

#endif
