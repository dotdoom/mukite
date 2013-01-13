#ifndef WORKER_H
#define WORKER_H

#include <pthread.h>

#include "xmcomp/common.h"
#include "xmcomp/queue.h"
#include "xmcomp/ringbuffer.h"

#include "builder.h"
#include "rooms.h"

typedef struct {
	BOOL enabled;
	StanzaQueue *queue;
	RingBuffer *ringbuffer;
	int builder_buffer_size;
	int deciseconds_limit;
	Rooms *rooms;
	ACLConfig *acl;
	pthread_t thread;
} WorkerConfig;

void *worker_thread_entry(void *void_worker_config);

void worker_establish_local_storage();
BOOL worker_send(BuilderPacket *);
BOOL worker_bounce(IncomingPacket *, XMPPError *, BufferPtr *node);

#endif
