#ifndef READER_H
#define READER_H

#include "queue.h"

typedef struct {
	char enabled;
	int socket;
	char recovery;
	int recovery_stanza_size;
	StanzaQueue queue;
} ReaderConfig;

void *reader_thread_entry(void *);

#endif
