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
	int overflows, underflows, reads, writes;
} RingBufferStats;

typedef struct {
	char
		// begginning of the allocated memory area
		*start,
		// position from where data for reading is available;
		// any are before read_position and after (read_position+data_size)
		// is considered free
		*read_position;
	int
		// the size of the real data. Note that read_position+data_size
		// may exceed memory buffer end. This is the nature of a ring buffer
		data_size,
		// size of the memory area allocated
		buffer_size;

	// optimization
	char
		// end of the allocated memory area; in fact, start+buffer_size precomputed for speed
		*end,
		// position where the next data block may be written. In fact,
		// cycled (read_position+data_size) - precomputed for speed
		*write_position;

	RingBufferSync sync;
	RingBufferStats stats;

	// When FALSE, no new data is expected for the buffer;
	// this usually indicates that the component is shutting down
	BOOL online;
} RingBuffer;

void ringbuffer_init(RingBuffer *, char *, int);
void ringbuffer_clear(RingBuffer *);
void ringbuffer_destroy(RingBuffer *);

void ringbuffer_write(RingBuffer *, char *, int);

int ringbuffer_get_chunk(RingBuffer *);
void ringbuffer_release_chunk(RingBuffer *, int);

void ringbuffer_offline(RingBuffer *);

#endif
