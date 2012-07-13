#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>

#include "xmlfsm.h"
#include "network.h"
#include "queue.h"
#include "sighelper.h"
#include "logger.h"

#include "reader.h"

int realloc_cyclic_buffer(BufferPtr *buffer, BufferPtr *data, int new_size) {
	char *new_buffer = 0;
	int tail_size, data_size = (data->end > data->data) ?
		BPT_SIZE(data) : (BPT_SIZE(buffer) - BPT_SIZE(data));

	if (new_size < data_size) {
		LWARN("desired buffer size (%d) is smaller than the actual data size (%d), expanding",
				new_size, data_size);
		new_size = data_size;
	}

	new_buffer = malloc(new_size);
	if (!new_buffer) {
		LERROR("out of memory when allocating a buffer of size %d", new_size);
		return BPT_SIZE(buffer);
	}

	if (data->data != data->end) {
		if (data->data > data->end) {
			tail_size = buffer->end - data->data;
			memcpy(new_buffer, data->data, tail_size);
			memcpy(new_buffer + tail_size, buffer->data, data->end - buffer->data);
		} else {
			memcpy(new_buffer, data->data, BPT_SIZE(data));
		}
	}

	free(buffer->data);
	data->data = buffer->data = new_buffer;
	buffer->end = new_buffer + new_size;
	data->end = new_buffer + data_size;

	return new_size;
}

inline int get_continuous_block(BufferPtr *buffer, BufferPtr *data, char **continuous_buffer) {
	if (data->end == data->data) {
		data->end = data->data = buffer->data;
		*continuous_buffer = buffer->data;
		return BPT_SIZE(buffer);
	}

	if (data->end == buffer->end) {
		*continuous_buffer = buffer->data;
		return data->data - buffer->data;
	}

	if (data->end > data->data) {
		*continuous_buffer = data->end;
		return buffer->end - data->end;
	}

	*continuous_buffer = data->end;
	return data->data - data->end;
}

void *reader_thread_entry(void *void_config) {
	ReaderConfig *config = (ReaderConfig *)void_config;
	StanzaQueue *queue = &config->queue;
	StanzaQueueStats *stats = &queue->stats;

	StanzaEntry *current_stanza_entry = 0;

	BufferPtr network_buffer, network_data;
	int network_buffer_size = 0, network_data_size, current_stanza_size,
		desired_stanza_buffer_size, bytes_received, continuous_block_size;
	char *current_stanza = 0, *continuous_block = 0;

	LINFO("started");
	sighelper_sigblockall();

	network_buffer.data = malloc(network_buffer_size = queue->network_buffer_size);
	network_buffer.end = network_buffer.data + network_buffer_size;
	network_data.data = network_data.end = network_buffer.data;

	if (!network_buffer.data) {
		LERROR("failed to allocate network buffer of size %d", queue->network_buffer_size);
		pthread_exit(0);
	}

	LDEBUG("starting processor");
	while (config->enabled) {
		if (queue->network_buffer_size != network_buffer_size) {
			LINFO("attempting to reallocate network buffer from %d to %d bytes",
					network_buffer_size, queue->network_buffer_size);

			network_buffer_size =
				realloc_cyclic_buffer(&network_buffer, &network_data, queue->network_buffer_size);

			LINFO("network buffer has been reallocated to %d bytes", network_buffer_size);
		}

		/*if (config->recovery_mode) {
			xmlfsm_recover(&network_buffer, &network_data);
		}*/

		LDEBUG("trying to parse stanza from the received buffer");
		current_stanza = network_data.data;
		while (xmlfsm_skip_node(&network_data, 0, &network_buffer) == XMLPARSE_SUCCESS) {
			LDEBUG("received stanza: '%.*s<!-- cyclic buffer rev split -->%.*s'",
					(int)((current_stanza < network_data.data) ?
						0 : network_data.data - network_buffer.data),
					network_buffer.data,
					(int)(((current_stanza < network_data.data) ?
						network_data.data : network_buffer.end) - current_stanza),
					current_stanza);

			current_stanza_entry = queue_pop_free(queue);
			current_stanza_size = network_data.data - current_stanza;
			if (current_stanza_size < 0) {
				current_stanza_size = network_buffer_size - current_stanza_size;
			}
			network_data_size -= current_stanza_size;

			if (!(desired_stanza_buffer_size = queue->fixed_block_buffer_size)) {
				desired_stanza_buffer_size = current_stanza_size;
			}
			if (current_stanza_entry->buffer_size != desired_stanza_buffer_size) {
				if (current_stanza_entry->buffer) {
					current_stanza_entry->buffer = realloc(current_stanza_entry->buffer, desired_stanza_buffer_size);
					if (current_stanza_entry->buffer_size > desired_stanza_buffer_size) {
						++stats->realloc_enlarges;
					} else {
						++stats->realloc_shortens;
					}
				} else {
					current_stanza_entry->buffer = malloc(desired_stanza_buffer_size);
					++stats->mallocs;
				}
				if (!current_stanza_entry->buffer) {
					LERROR("failed to (re)allocate a buffer of size %d for the stanza",
							desired_stanza_buffer_size);
					current_stanza_entry->buffer_size = 0;
				} else {
					current_stanza_entry->buffer_size = desired_stanza_buffer_size;
				}
			}

			if (current_stanza_entry->buffer_size < current_stanza_size) {
				LWARN("received stanza size %d exceeds allocated queue buffer size %d, dropped both",
						current_stanza_size, current_stanza_entry->buffer_size);
				current_stanza_entry->data_size = 0;
			} else {
				if (network_data.data > current_stanza) {
					memcpy(current_stanza_entry->buffer, current_stanza, current_stanza_size);
				} else {
					memcpy(current_stanza_entry->buffer, current_stanza,
							continuous_block_size = network_buffer.end - current_stanza);
					memcpy(current_stanza_entry->buffer + continuous_block_size,
							network_buffer.data, network_data.data - network_buffer.data);
				}
				current_stanza_entry->data_size = current_stanza_size;
			}

			LDEBUG("pushing stanza sized %d into the queue", current_stanza_entry->data_size);
			queue_push_data(queue, current_stanza_entry);
			current_stanza = network_data.data;
		}

		if (!(continuous_block_size = get_continuous_block(&network_buffer, &network_data, &continuous_block))) {
			LDEBUG("network buffer overflow: '%.*s'", network_buffer_size, network_buffer.data);
			if (queue->network_buffer_size < (1 << 20)) {
				queue->network_buffer_size = (1 << 20);
				LWARN("network buffer was unable to keep one stanza and will be forced from %d to %d bytes",
						network_buffer_size, queue->network_buffer_size);
				continue;
			} else {
				LERROR("fatality: network buffer (size %d) is not large enough to keep a single stanza! Bailing out",
						network_buffer_size);
				break;
			}
		}

		LDEBUG("reading %d bytes from network", continuous_block_size);
		bytes_received = net_recv(config->socket, continuous_block, continuous_block_size);

		if (bytes_received < 0 || !config->socket->connected) {
			LERROR("unrecoverable network error detected, exiting");
			break;
		}

		network_data.end += bytes_received;
		if (network_data.end > network_buffer.end) {
			network_data.end -= network_buffer_size;
		}

		current_stanza = network_data.data;
	}
	config->enabled = 0;

	LINFO("terminated");
	pthread_exit(0);
}
