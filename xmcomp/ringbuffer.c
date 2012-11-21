#include <stdlib.h>
#include <string.h>

#include "common.h"

#include "ringbuffer.h"

void ringbuffer_init(RingBuffer *ringbuffer, char *buffer, int size) {
	RingBufferSync *sync = &ringbuffer->sync;

	ringbuffer->start = ringbuffer->read_position = ringbuffer->write_position = buffer;
	ringbuffer->buffer_size = size;
	ringbuffer->data_size = 0;
	ringbuffer->end = ringbuffer->start + ringbuffer->buffer_size;
	ringbuffer->write_size = ringbuffer->buffer_size;

	pthread_mutex_init(&sync->ringbuffer_mutex, 0);
	pthread_cond_init(&sync->data_available_cv, 0);
	pthread_cond_init(&sync->free_available_cv, 0);

	ringbuffer->online = TRUE;
}

void ringbuffer_clear(RingBuffer *ringbuffer) {
	ringbuffer->read_position = ringbuffer->write_position = ringbuffer->start;
	ringbuffer->data_size = 0;
	ringbuffer->write_size = ringbuffer->buffer_size;
}

void ringbuffer_destroy(RingBuffer *ringbuffer) {
	RingBufferSync *sync = &ringbuffer->sync;
	pthread_mutex_destroy(&sync->ringbuffer_mutex);
	pthread_cond_destroy(&sync->data_available_cv);
	pthread_cond_destroy(&sync->free_available_cv);
}

inline void ringbuffer_write(RingBuffer *ringbuffer, char *buffer, int size) {
	RingBufferSync *sync = &ringbuffer->sync;
	RingBufferStats *stats = &ringbuffer->stats;

	int write_size = 0;
	pthread_mutex_lock(&ringbuffer->sync.ringbuffer_mutex);
	while (size > 0) {
		while (!ringbuffer->write_size) {
			++stats->overflows;
			pthread_cond_wait(&sync->free_available_cv, &sync->ringbuffer_mutex);
		}

		write_size = (ringbuffer->write_size > size) ? size : ringbuffer->write_size;
		memcpy(ringbuffer->write_position, buffer, write_size);

		ringbuffer->write_position += write_size;
		if (ringbuffer->write_position > ringbuffer->end) {
			ringbuffer->write_position = ringbuffer->start;
			ringbuffer->write_size = ringbuffer->read_position - ringbuffer->start;
		} else {
			ringbuffer->write_size -= write_size;
		}
		ringbuffer->data_size += write_size;

		size -= write_size;
		buffer += write_size;

		pthread_cond_signal(&sync->data_available_cv);
	}
	++stats->reads;
	pthread_mutex_unlock(&ringbuffer->sync.ringbuffer_mutex);
}

inline void ringbuffer_offline(RingBuffer *ringbuffer) {
	RingBufferSync *sync = &ringbuffer->sync;

	ringbuffer->online = FALSE;
	// Also wakeup a waiting condvar for the case there's no data
	pthread_cond_signal(&sync->data_available_cv);
}

inline int ringbuffer_get_chunk(RingBuffer *ringbuffer) {
	RingBufferSync *sync = &ringbuffer->sync;
	RingBufferStats *stats = &ringbuffer->stats;
	int end_buffer_delta;

	while (!ringbuffer->data_size) {
		if (!ringbuffer->online) {
			return 0;
		}
		++stats->underflows;
		pthread_mutex_lock(&sync->ringbuffer_mutex);
		pthread_cond_wait(&sync->data_available_cv, &sync->ringbuffer_mutex);
		pthread_mutex_unlock(&sync->ringbuffer_mutex);
	}

	end_buffer_delta = ringbuffer->end - ringbuffer->read_position;
	if (end_buffer_delta > ringbuffer->data_size) {
		return ringbuffer->data_size;
	}
	return end_buffer_delta;
}

inline void ringbuffer_release_chunk(RingBuffer *ringbuffer, int size) {
	RingBufferSync *sync = &ringbuffer->sync;
	
	pthread_mutex_lock(&sync->ringbuffer_mutex);

	if (ringbuffer->read_position > ringbuffer->write_position) {
		ringbuffer->write_size += size;
	}

	ringbuffer->read_position += size;
	if (ringbuffer->read_position == ringbuffer->end) {
		ringbuffer->read_position = ringbuffer->start;
	}
	ringbuffer->data_size -= size;

	pthread_cond_signal(&sync->free_available_cv);
	pthread_mutex_unlock(&sync->ringbuffer_mutex);
}
