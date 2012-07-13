#include <stdlib.h>
#include <string.h>

#include "common.h"

#include "cbuffer.h"

void cbuffer_init(CBuffer *cbuffer, char *buffer, int size) {
	CBufferSync *sync = &cbuffer->sync;

	cbuffer->start = cbuffer->read_position = cbuffer->write_position = buffer;
	cbuffer->buffer_size = size;
	cbuffer->data_size = 0;
	cbuffer->end = cbuffer->start + cbuffer->buffer_size;
	cbuffer->write_size = cbuffer->buffer_size;

	pthread_mutex_init(&sync->cbuffer_mutex, 0);
	pthread_cond_init(&sync->data_available_cv, 0);
	pthread_cond_init(&sync->free_available_cv, 0);
}

void cbuffer_clear(CBuffer *cbuffer) {
	cbuffer->read_position = cbuffer->write_position = cbuffer->start;
	cbuffer->data_size = 0;
	cbuffer->write_size = cbuffer->buffer_size;
}

inline void cbuffer_write(CBuffer *cbuffer, char *buffer, int size) {
	CBufferSync *sync = &cbuffer->sync;
	CBufferStats *stats = &cbuffer->stats;

	int write_size = 0;
	pthread_mutex_lock(&cbuffer->sync.cbuffer_mutex);
	while (size > 0) {
		while (!cbuffer->write_size) {
			++stats->overflows;
			pthread_cond_wait(&sync->free_available_cv, &sync->cbuffer_mutex);
		}

		write_size = (cbuffer->write_size > size) ? size : cbuffer->write_size;
		memcpy(cbuffer->write_position, buffer, write_size);

		cbuffer->write_position += write_size;
		if (cbuffer->write_position > cbuffer->end) {
			cbuffer->write_position = cbuffer->start;
			cbuffer->write_size = cbuffer->read_position - cbuffer->start;
		} else {
			cbuffer->write_size -= write_size;
		}
		cbuffer->data_size += write_size;

		size -= write_size;
		buffer += write_size;

		pthread_cond_signal(&sync->data_available_cv);
	}
	pthread_mutex_unlock(&cbuffer->sync.cbuffer_mutex);
}

inline int cbuffer_get_chunk(CBuffer *cbuffer) {
	CBufferSync *sync = &cbuffer->sync;
	CBufferStats *stats = &cbuffer->stats;
	int end_buffer_delta;

	while (!cbuffer->data_size) {
		++stats->underflows;
		pthread_mutex_lock(&sync->cbuffer_mutex);
		pthread_cond_wait(&sync->data_available_cv, &sync->cbuffer_mutex);
		pthread_mutex_unlock(&sync->cbuffer_mutex);
	}

	end_buffer_delta = cbuffer->end - cbuffer->read_position;
	if (end_buffer_delta > cbuffer->data_size) {
		return cbuffer->data_size;
	}
	return end_buffer_delta;
}

inline void cbuffer_release_chunk(CBuffer *cbuffer, int size) {
	CBufferSync *sync = &cbuffer->sync;
	
	pthread_mutex_lock(&sync->cbuffer_mutex);

	if (cbuffer->read_position > cbuffer->write_position) {
		cbuffer->write_size += size;
	}

	cbuffer->read_position += size;
	if (cbuffer->read_position == cbuffer->end) {
		cbuffer->read_position = cbuffer->start;
	}
	cbuffer->data_size -= size;

	pthread_cond_signal(&sync->free_available_cv);
	pthread_mutex_unlock(&sync->cbuffer_mutex);
}
