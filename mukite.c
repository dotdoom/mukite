#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "xmcomp/network.h"
#include "xmcomp/sighelper.h"
#include "xmcomp/logger.h"

#include "config.h"

#define APP_NAME "mukite"

Config config;
BOOL running = TRUE;

static void reload_config(int signal) {
	config_read(&config);
	config_apply(&config);
}

static void save_data(int signal) {
	int error;
	FILE *output = 0;
	LINFO("received signal %d, saving to the file: '%s'", signal, config.parser.data_file);
	if ((output = fopen(config.parser.data_file, "w"))) {
		if (!rooms_serialize(&config.rooms, output)) {
			LERROR("serialization failure, probably disk error");
		}
		fclose(output);
	} else {
		error = errno;
		LERRNO("failed to save data", error);
	}
}

static void load_data() {
	int error;
	FILE *input = 0;
	LINFO("loading from the file: '%s'", config.parser.data_file);
	if ((input = fopen(config.parser.data_file, "r"))) {
		if (!rooms_deserialize(&config.rooms, input, 100)) {
			LFATAL("deserialization failure, probably disk error/version mismatch;\n"
					"please rename or remove the data file to start from scratch");
		}
		fclose(input);
	} else {
		error = errno;
		LERRNO("failed to load data", error);
	}
}

static void terminate(int signal) {
	save_data(signal);
	running = FALSE;

	if (config.reader_thread.enabled) {
		// SIGUSR2 the reader thread to interrupt the recv() call
		config.reader_thread.enabled = FALSE;
		pthread_kill(config.reader_thread.thread, SIGUSR2);

		// As soon as reader thread is terminated, main() terminates the writer thread and wraps up
	}
}

int main(int argc, char **argv) {
	// For connect/authenticate failures
	int reconnect_delay = 1;

	// RingBuffer buffer for the writer
	char *writer_buffer = 0;

	LINFO("%s starting", APP_NAME);

	config_init(&config, argc > 1 ? argv[1] : 0);
	if (!config_read(&config)) {
		return 1;
	}

	load_data();

	sighelper_sigaction(SIGHUP, reload_config);
	sighelper_sigaction(SIGUSR1, save_data);
	sighelper_sigaction(SIGTERM, terminate);
	sighelper_sigaction(SIGQUIT, terminate);
	sighelper_sigaction(SIGINT, terminate);

	while (running) {
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

		// Display component hostname in process list - user convenience
		strcpy(argv[0], APP_NAME " ");
		strncat(argv[0], config.component.hostname, CONFIG_OPTION_LENGTH);

		LDEBUG("allocating writer buffer, size %d", config.writer.buffer);
		writer_buffer = malloc(config.writer.buffer);
		ringbuffer_init(&config.writer_thread.ringbuffer,
				writer_buffer, config.writer.buffer);

		LDEBUG("creating reader queue, size %d", config.reader.queue);
		queue_init(&config.reader_thread.queue, config.reader.queue);

		LINFO("creating writer thread");
		config.writer_thread.socket = &config.socket;
		config.writer_thread.enabled = TRUE;
		pthread_create(&config.writer_thread.thread, 0, writer_thread_entry, (void *)&config.writer_thread);

		LINFO("creating reader thread");
		config.reader_thread.socket = &config.socket;
		config.reader_thread.enabled = TRUE;
		pthread_create(&config.reader_thread.thread, 0, reader_thread_entry, (void *)&config.reader_thread);

		LINFO("creating worker threads");
		config_apply(&config);

		LINFO("started");
		LDEBUG("joining reader thread");
		pthread_join(config.reader_thread.thread, 0);
		// Switch ringbuffer to offline, indicating no more data is expected.
		// As soon as the writer finishes the job, it will terminate
		ringbuffer_offline(&config.writer_thread.ringbuffer);
		LDEBUG("joining writer thread");
		pthread_join(config.writer_thread.thread, 0);

		LINFO("destroying buffers, queues and disconnecting");
		ringbuffer_destroy(&config.writer_thread.ringbuffer);
		queue_destroy(&config.reader_thread.queue);

		net_unstream(&config.socket);
		net_disconnect(&config.socket);
	}

	return 0;
}
