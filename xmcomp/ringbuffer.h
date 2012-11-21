/*
 * This implements a ring buffer
 */

#ifndef XMCOMP_RINGBUFFER_H
#define XMCOMP_RINGBUFFER_H

#include <pthread.h>

#include "common.h"

typedef struct {
	pthread_mutex_t ringbuffer_mutex;
	pthread_cond_t data_available_cv;
	pthread_cond_t free_available_cv;
} RingBufferSync;

typedef struct {
	int overflows, underflows, reads;
} RingBufferStats;

typedef struct {
	// Write position can be evaled but saved for the speed
	char *start, *read_position;
	int data_size, buffer_size;

	// Optimization
	char *end, *write_position;
	int write_size;

	RingBufferSync sync;
	RingBufferStats stats;

	// When FALSE, no new data is expected for the buffer;
	// this usually indicates that the component is shutting down
	BOOL online;
} RingBuffer;

void ringbuffer_init(RingBuffer *, char *, int);
void ringbuffer_clear(RingBuffer *);
void ringbuffer_destroy(RingBuffer *);

inline void ringbuffer_write(RingBuffer *, char *, int);

inline int ringbuffer_get_chunk(RingBuffer *);
inline void ringbuffer_release_chunk(RingBuffer *, int);

inline void ringbuffer_offline(RingBuffer *);

#endif
