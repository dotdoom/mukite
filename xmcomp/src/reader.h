#ifndef XMCOMP_READER_H
#define XMCOMP_READER_H

#include <pthread.h>

#include "common.h"
#include "network.h"
#include "queue.h"

typedef struct {
	BOOL enabled;
	Socket *socket;
	StanzaQueue queue;
	pthread_t thread;
} ReaderConfig;

void *reader_thread_entry(void *);

#endif
