#ifndef XMCOMP_READER_H
#define XMCOMP_READER_H

#include "queue.h"

typedef struct {
	char enabled;
	int socket;
	char recovery_mode;
	int recovery_stanza_size;
	StanzaQueue queue;
} ReaderConfig;

void *reader_thread_entry(void *);

#endif
