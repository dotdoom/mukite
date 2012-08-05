#ifndef XMCOMP_WRITER_H
#define XMCOMP_WRITER_H

#include "network.h"
#include "cbuffer.h"

typedef struct {
	BOOL enabled;
	Socket *socket;
	CBuffer cbuffer;
} WriterConfig;

void *writer_thread_entry(void *);

#endif
