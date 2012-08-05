#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "xmcomp/network.h"
#include "xmcomp/sighelper.h"
#include "xmcomp/logger.h"

#include "config.h"
//#include "writer.h"
//#include "xcwrapper.h"

#define APP_NAME "mukite"

Config config;

static void reload_config(int signal) {
	config_read(&config);
	config_apply(&config);
}

static void dump_data(int signal) {
}

static void terminate(int signal) {
//	dump_data(signal);
//	running = 0;
}

int main(int argc, char **argv) {
	// For connect/authenticate failures
	int reconnect_delay = 1;

	// CBuffer buffer for the writer
	char *writer_buffer = 0;

	LINFO("%s %d starting", APP_NAME, VERSION);

	config_init(&config, argc > 1 ? argv[1] : 0);
	if (!config_read(&config)) {
		return 1;
	}
	
	sighelper_sigaction(SIGHUP, reload_config);
	sighelper_sigaction(SIGUSR1, dump_data);
	sighelper_sigaction(SIGTERM, terminate);

	while (1) {
		LINFO("connecting to %s:%d", config.network.host, config.network.port);

		if (net_connect(&config.socket, config.network.host, config.network.port)) {
			LINFO("opening XMPP stream to %s", config.component.hostname);
			if (!net_stream(&config.socket,
						"xmcomp",
						config.component.hostname,
						config.component.password)) {
				net_disconnect(&config.socket);
			}
		}

		if (!config.socket.connected) {
			LERROR("retrying in %d second(s)", reconnect_delay);
			sleep(reconnect_delay);
			if (reconnect_delay < 60) {
				reconnect_delay <<= 1;
			}
			continue;
		}
		reconnect_delay = 1;

		// Display component hostname is process list - user convenience
		strcpy(argv[0], APP_NAME " ");
		strncat(argv[0], config.component.hostname, CONFIG_OPTION_LENGTH);

		LDEBUG("allocating writer buffer, size %d", config.writer.buffer);
		writer_buffer = malloc(config.writer.buffer);
		cbuffer_init(&config.writer_thread.cbuffer,
				writer_buffer, config.writer.buffer);

		LDEBUG("creating reader queue, size %d", config.reader.queue);
		queue_init(&config.reader_thread.queue, config.reader.queue);

		LINFO("creating writer thread");
		config.writer_thread.socket = &config.socket;
		config.writer_thread.enabled = TRUE;
		pthread_create(&config.writer_thread_id, 0, writer_thread_entry, (void *)&config.writer_thread);

		LINFO("creating reader thread");
		config.reader_thread.socket = &config.socket;
		config.reader_thread.enabled = TRUE;
		pthread_create(&config.reader_thread_id, 0, reader_thread_entry, (void *)&config.reader_thread);

		LINFO("creating worker threads");
		config_apply(&config);

		LINFO("WAITING");
		pthread_join(config.writer_thread_id, 0);

		LINFO("destroying buffers, queues and disconnecting");
		cbuffer_destroy(&config.writer_thread.cbuffer);
		queue_destroy(&config.reader_thread.queue);

		net_unstream(&config.socket);
		net_disconnect(&config.socket);
	}

	return 0;
}
