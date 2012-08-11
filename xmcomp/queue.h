/*
 * This is an implementation of a 2-stage queue
 */

#ifndef XMCOMP_QUEUE_H
#define XMCOMP_QUEUE_H

#include <pthread.h>

typedef struct StanzaEntry {
	// A buffer that keeps actual data
	char *buffer;

	int
		// Number of real data bytes in the buffer
		data_size,
		// Actual buffer size.
		// Sometimes buffer is preallocated larger than data for performance reasons
		buffer_size;

	// Linked list
	struct StanzaEntry *next;
} StanzaEntry;

typedef struct {
	int
		// Number of times a thread had to wait to enqueue an item
		overflows,
		// Parser thread had to wait for some work to appear
		underflows,
		// Single block reallocations to enlarge the dynamic buffer
		realloc_enlarges,
		// Reallocations to shorten the dynamic buffer
		realloc_shortens,
		// Initial block buffer memory allocations
		mallocs;
} StanzaQueueStats;

typedef struct {
	pthread_mutex_t data_queue_mutex;
	pthread_mutex_t free_queue_mutex;

	// Cond var signalling that the new portion of data is available for parsing
	pthread_cond_t data_available_cv;
	// Signals that a previously busy block is available for being filled
	pthread_cond_t free_available_cv;
} StanzaQueueSync;

typedef struct {
	StanzaEntry
		// Stanza blocks filled with data ready to parse
		*data_start_queue, *data_end_queue,
		// Free blocks to receive data
		*free_queue;

	StanzaQueueStats stats;
	StanzaQueueSync sync;

	// When non-zero, this denotes the block size for every single item in the queue
	int fixed_block_buffer_size;

	// The desired size of a network buffer. MUST ALWAYS BE AT LEAST THE SIZE OF A SIGNLE STANZA
	int network_buffer_size;
} StanzaQueue;

void queue_init(StanzaQueue *, int);
void queue_destroy(StanzaQueue *);

inline StanzaEntry *queue_pop_free(StanzaQueue *);
inline void queue_push_data(StanzaQueue *, StanzaEntry *);

inline StanzaEntry *queue_pop_data(StanzaQueue *);
inline void queue_push_free(StanzaQueue *, StanzaEntry *);

#endif
