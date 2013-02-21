#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "xmcomp/src/network.h"
#include "xmcomp/src/sighelper.h"
#include "xmcomp/src/logger.h"
#include "xmcomp/src/writer.h"
#include "xmcomp/src/reader.h"

#include "worker.h"
#include "config.h"
#include "acl.h"
#include "timer.h"

#define WORKERS_SIZE_LIMIT 1024
#define ROOMS_SIZE_LIMIT 10000
#define ACLS_SIZE_LIMIT 1024

BOOL running = TRUE;

WriterConfig writer_thread;
ReaderConfig reader_thread;
struct {
	int size;
	WorkerConfig threads[WORKERS_SIZE_LIMIT];
} worker_threads;

Config config;
ACLConfig acl;
Rooms rooms;

static void apply_config() {
	WorkerConfig *worker = 0;
	FILE *acl_data_file = 0;
	int i, error;

	LDEBUG("applying configuration settings");

	log_level = config.logger.level;

	reader_thread.queue.fixed_block_buffer_size =
		config.reader.block;
	reader_thread.queue.network_buffer_size =
		config.reader.buffer;
	//rooms.deciseconds_limit = config.worker.deciseconds_limit;

	acl.default_role = config.acl.default_role;
	if ((acl_data_file = fopen(config.acl.data_file, "r"))) {
		acl_deserialize(&acl, acl_data_file, ACLS_SIZE_LIMIT);
		fclose(acl_data_file);
	} else {
		error = errno;
		LERRNO("could not open acl data file '%s' for reading, leaving ACLs untouched",
				error, config.acl.data_file);
	}

	if (config.worker.threads > WORKERS_SIZE_LIMIT) {
		LERROR("%d exceeds workers limit %d, shrinking",
				config.worker.threads, WORKERS_SIZE_LIMIT);
		config.worker.threads = WORKERS_SIZE_LIMIT;
	}

	if (worker_threads.size < config.worker.threads) {
		for (i = worker_threads.size; i < config.worker.threads; ++i) {
			worker = &worker_threads.threads[i];
			worker->queue = &reader_thread.queue;
			worker->ringbuffer = &writer_thread.ringbuffer;
			worker->rooms = &rooms;
			worker->acl = &acl;
			worker->hostname.data = config.component.hostname;
			worker->hostname.end = worker->hostname.data + strlen(config.component.hostname);
			worker->enabled = FALSE;
		}
	} else if (worker_threads.size > config.worker.threads) {
		for (i = config.worker.threads; i < worker_threads.size; ++i) {
			worker = &worker_threads.threads[i];
			worker->enabled = FALSE;
		}
	}
	worker_threads.size = config.worker.threads;

	for (i = 0; i < worker_threads.size; ++i) {
		worker = &worker_threads.threads[i];
		worker->builder_buffer_size = config.worker.buffer;
		if (!worker->enabled) {
			worker->enabled = TRUE;
			pthread_create(&worker->thread, 0, worker_thread_entry, (void *)worker);
		}
	}
}

static void reload_config(int signal) {
	config_read(&config);
	apply_config();
}

static void serialize_data(int signal) {
	int error;
	FILE *output = 0;
	LINFO("received signal %d, serializing to the file: '%s'", signal, config.worker.data_file);
	if ((output = fopen(config.worker.data_file, "w"))) {
		if (!rooms_serialize(&rooms, output)) {
			LERROR("serialization failure, probably disk error");
		}
		fclose(output);
	} else {
		error = errno;
		LERRNO("failed to open file for data serialization", error);
	}
}

static void deserialize_data() {
	int error;
	FILE *input = 0;
	LINFO("deserializing from the file: '%s'", config.worker.data_file);
	if ((input = fopen(config.worker.data_file, "r"))) {
		if (!rooms_deserialize(&rooms, input)) {
			LFATAL("deserialization failure, probably disk error/version mismatch;\n"
					"please rename or remove the data file to start from scratch");
		}
		fclose(input);
	} else {
		error = errno;
		LERRNO("failed to open file for data deserialization", error);
	}
}

static void terminate(int signal) {
	serialize_data(signal);
	running = FALSE;

	if (reader_thread.enabled) {
		// SIGUSR2 the reader thread to interrupt the recv() call
		reader_thread.enabled = FALSE;
		pthread_kill(reader_thread.thread, SIGUSR2);
		// As soon as reader thread is terminated, main() terminates the writer thread and wraps up
	}
}

int main(int argc, char **argv) {
	// For connect/authenticate failures
	int reconnect_delay = 1;

	// RingBuffer buffer for the writer
	char *writer_buffer = 0;

	Socket socket;

	LINFO("starting");
	timer_start();
	worker_establish_local_storage();

	config_init(&config, argc > 1 ? argv[1] : 0);
	if (!config_read(&config)) {
		return 1;
	}

	acl_init(&acl);
	rooms_init(&rooms);
	deserialize_data();

	sighelper_sigaction(SIGHUP, reload_config);
	sighelper_sigaction(SIGUSR1, serialize_data);
	sighelper_sigaction(SIGTERM, terminate);
	sighelper_sigaction(SIGQUIT, terminate);
	sighelper_sigaction(SIGINT, terminate);

	LDEBUG("allocating writer buffer, size %d", config.writer.buffer);
	writer_buffer = malloc(config.writer.buffer);
	ringbuffer_init(&writer_thread.ringbuffer,
			writer_buffer, config.writer.buffer);

	LDEBUG("creating reader queue, size %d", config.reader.queue);
	queue_init(&reader_thread.queue, config.reader.queue);

	while (running) {
		LINFO("connecting to %s:%d", config.network.host, config.network.port);

		if (net_connect(&socket, config.network.host, config.network.port)) {
			LINFO("opening XMPP stream to %s", config.component.hostname);
			if (!net_stream(&socket,
						"xmcomp",
						config.component.hostname,
						config.component.password)) {
				net_disconnect(&socket);
			}
		}

		if (!socket.connected) {
			LERROR("retrying in %d second(s)", reconnect_delay);
			sleep(reconnect_delay);
			if (reconnect_delay < 60) {
				reconnect_delay <<= 1;
			}
			continue;
		}
		reconnect_delay = 1;

		apply_config();

		LINFO("creating writer thread");
		writer_thread.socket = &socket;
		writer_thread.enabled = TRUE;
		pthread_create(&writer_thread.thread, 0, writer_thread_entry, (void *)&writer_thread);

		LINFO("creating reader thread");
		reader_thread.socket = &socket;
		reader_thread.enabled = TRUE;
		pthread_create(&reader_thread.thread, 0, reader_thread_entry, (void *)&reader_thread);

		LINFO("started");
		LDEBUG("joining reader thread");
		pthread_join(reader_thread.thread, 0);
		// Switch ringbuffer to offline, indicating no more data is expected.
		// As soon as the writer finishes the job, it will terminate
		ringbuffer_offline(&writer_thread.ringbuffer);
		LDEBUG("joining writer thread");
		pthread_join(writer_thread.thread, 0);

		LINFO("clearing output buffer and disconnecting");
		ringbuffer_clear(&writer_thread.ringbuffer);

		net_unstream(&socket);
		net_disconnect(&socket);
	}

	LINFO("cleaning up");
	ringbuffer_destroy(&writer_thread.ringbuffer);
	queue_destroy(&reader_thread.queue);
	free(writer_buffer);

	return 0;
}
