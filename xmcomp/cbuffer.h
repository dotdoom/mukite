/*
 * This implements a cyclic buffer
 */

#ifndef XMCOMP_CBUFFER_H
#define XMCOMP_CBUFFER_H

#include <pthread.h>

typedef struct {
	pthread_mutex_t cbuffer_mutex;
	pthread_cond_t data_available_cv;
	pthread_cond_t free_available_cv;
} CBufferSync;

typedef struct {
	int overflows, underflows;
} CBufferStats;

typedef struct {
	// Write position can be evaled but saved for the speed
	char *start, *read_position;
	int data_size, buffer_size;

	// Optimization
	char *end, *write_position;
	int write_size;

	CBufferSync sync;
	CBufferStats stats;
} CBuffer;

void cbuffer_init(CBuffer *, char *, int);
void cbuffer_clear(CBuffer *);

inline void cbuffer_write(CBuffer *, char *, int);

inline int cbuffer_get_chunk(CBuffer *);
inline void cbuffer_release_chunk(CBuffer *, int);

#endif
