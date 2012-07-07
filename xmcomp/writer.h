#ifndef XMCOMP_WRITER_H
#define XMCOMP_WRITER_H

#include "cbuffer.h"

typedef struct {
	char enabled;
	int socket;
	CBuffer cbuffer;
} WriterConfig;

void *writer_thread_entry(void *);

#endif
