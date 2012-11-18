#ifndef XMCOMP_WRITER_H
#define XMCOMP_WRITER_H

#include <pthread.h>

#include "network.h"
#include "cbuffer.h"

typedef struct {
	BOOL enabled;
	Socket *socket;
	CBuffer cbuffer;
	pthread_t thread;
} WriterConfig;

void *writer_thread_entry(void *);

#endif
