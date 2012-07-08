#include <pthread.h>
#include <signal.h>

#include "network.h"
#include "cbuffer.h"
#include "sighelper.h"

#include "writer.h"

void *writer_thread_entry(void *void_config) {
	WriterConfig *config = (WriterConfig *)void_config;
	CBuffer *cbuffer = &config->cbuffer;
	int size;
	LINFO("started");
	sighelper_sigblockall();

	while (config->enabled) {
		size = cbuffer_get_chunk(cbuffer);
		size = net_send(&config->socket, cbuffer->read_position, size);
		if (size < 0 || !config->socket.connected) {
			LERROR("unrecoverable network error detected, exiting");
			break;
		}
		cbuffer_release_chunk(cbuffer, size);
	}
	config->enabled = 0;

	LINFO("terminated");
	pthread_exit(0);
}
