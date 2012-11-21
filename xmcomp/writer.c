#include "network.h"
#include "ringbuffer.h"
#include "sighelper.h"
#include "logger.h"

#include "writer.h"

void *writer_thread_entry(void *void_config) {
	WriterConfig *config = (WriterConfig *)void_config;
	RingBuffer *ringbuffer = &config->ringbuffer;
	int size;
	LINFO("started");
	sighelper_sigblockall(0);

	while (config->enabled) {
		if (!(size = ringbuffer_get_chunk(ringbuffer))) {
			LINFO("no more data to write (ringbuffer offline) - terminating");
			break;
		}
		size = net_send(config->socket, ringbuffer->read_position, size);
		if (size < 0 || !config->socket->connected) {
			LERROR("unrecoverable network error detected, exiting");
			break;
		}
		ringbuffer_release_chunk(ringbuffer, size);
	}
	config->enabled = FALSE;

	LINFO("terminated");
	pthread_exit(0);
}
