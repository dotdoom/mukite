#include <stdlib.h>
#include <string.h>

#include "queue.h"

/*
 * Take and remove the first free block from the free_queue
 */
inline StanzaEntry *queue_pop_free(StanzaQueue *queue) {
	StanzaEntry *block = 0;
	StanzaQueueSync *sync = &queue->sync;
	StanzaQueueStats *stats = &queue->stats;

	pthread_mutex_lock(&sync->free_queue_mutex);
	while (!queue->free_queue) {
		++stats->overflows;
		pthread_cond_wait(&sync->free_available_cv, &sync->free_queue_mutex);
	}
	block = queue->free_queue;
	queue->free_queue = block->next;
	++stats->free_pops;
	pthread_mutex_unlock(&sync->free_queue_mutex);

	return block;
}

/*
 * Take and remove the first block from data_start_queue
 */
inline StanzaEntry *queue_pop_data(StanzaQueue *queue) {
	StanzaEntry *block = 0;
	StanzaQueueSync *sync = &queue->sync;
	StanzaQueueStats *stats = &queue->stats;

	pthread_mutex_lock(&sync->data_queue_mutex);
	while (!queue->data_start_queue) {
		++stats->underflows;
		pthread_cond_wait(&sync->data_available_cv, &sync->data_queue_mutex);
	}
	block = queue->data_start_queue;
	queue->data_start_queue = block->next;
	if (queue->data_end_queue == block) { // && queue->data_start_queue == 0
		queue->data_end_queue = 0;
	}
	++stats->data_pops;
	pthread_mutex_unlock(&sync->data_queue_mutex);

	return block;
}

/*
 * Push the stanza to the end of the data_queue
 */
inline void queue_push_data(StanzaQueue *queue, StanzaEntry *stanza) {
	StanzaQueueSync *sync = &queue->sync;

	pthread_mutex_lock(&sync->data_queue_mutex);
	stanza->next = 0;
	if (queue->data_end_queue) {
		queue->data_end_queue->next = stanza;
	} else {
		queue->data_start_queue = stanza;
	}
	queue->data_end_queue = stanza;
	++queue->stats.data_pushes;
	pthread_cond_signal(&sync->data_available_cv);
	pthread_mutex_unlock(&sync->data_queue_mutex);
}

/*
 * Push a stanza to the free_queue
 */
inline void queue_push_free(StanzaQueue *queue, StanzaEntry *stanza) {
	StanzaQueueSync *sync = &queue->sync;

	pthread_mutex_lock(&sync->free_queue_mutex);
	stanza->next = queue->free_queue;
	queue->free_queue = stanza;
	pthread_cond_signal(&sync->free_available_cv);
	++queue->stats.free_pushes;
	pthread_mutex_unlock(&sync->free_queue_mutex);
}

void queue_init(StanzaQueue *queue, int size) {
	StanzaEntry *current = 0;
	StanzaQueueSync *sync = &queue->sync;
	int i;

	queue->data_start_queue =
		queue->data_end_queue = 0;

	queue->free_queue = 0;
	for (i = 0; i < size; ++i) {
		current = malloc(sizeof(*current));
		memset(current, 0, sizeof(*current));
		current->next = queue->free_queue;
		queue->free_queue = current;
	}

	pthread_mutex_init(&sync->data_queue_mutex, 0);
	pthread_mutex_init(&sync->free_queue_mutex, 0);
	pthread_cond_init(&sync->data_available_cv, 0);
	pthread_cond_init(&sync->free_available_cv, 0);
}

void queue_destroy_single(StanzaEntry *q) {
	StanzaEntry *c = q;
	while ((c = q)) {
		if (c->buffer) {
			free(c->buffer);
		}
		q = c->next;
		free(c);
	}
}

void queue_destroy(StanzaQueue *queue) {
	StanzaQueueSync *sync = &queue->sync;

	pthread_mutex_destroy(&sync->data_queue_mutex);
	pthread_mutex_destroy(&sync->free_queue_mutex);
	pthread_cond_destroy(&sync->data_available_cv);
	pthread_cond_destroy(&sync->free_available_cv);
	
	queue_destroy_single(queue->data_start_queue);
	queue_destroy_single(queue->free_queue);
}
