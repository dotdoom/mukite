#ifndef WORKER_H
#define WORKER_H

#include <pthread.h>

#include "xmcomp/common.h"

typedef struct {
	BOOL enabled;
	pthread_t thread;
} WorkerConfig;

void *worker_thread_entry(void *void_worker_config);

#endif
