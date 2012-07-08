#ifndef XMCOMP_READER_H
#define XMCOMP_READER_H

#include "network.h"
#include "queue.h"

typedef struct {
	char enabled;
	Socket *socket;
	char recovery_mode;
	int recovery_stanza_size;
	StanzaQueue queue;
} ReaderConfig;

void *reader_thread_entry(void *);

#endif
