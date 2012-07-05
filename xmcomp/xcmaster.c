#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>

#include "common.h"
#include "config.h"
#include "network.h"
#include "writer.h"
#include "xcwrapper.h"

XmcompConfig *config = 0;
pid_t wrapper = 0;

void reload_config(int signal) {
	config_read(stdin, config);
	if (wrapper > 0 && wrapper != (pid_t)-1) {
		kill(wrapper, SIGHUP);
	}
}

int master_main(XmcompConfig *, WriterConfig *) {
}

char *shm_alloc(char name_postfix, int size) {
	int fd;
	char *data;
	char name[CONFIG_OPTION_LENGTH] = "/xmcomp.";

	strncat(name, name_postfix, CONFIG_OPTION_LENGTH);
	fd = shm_open(name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	ftruncate(fd, size);
	data = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	shm_unlink(name);

	return data;
}

int main(int argc, char **argv) {
	// Shared memory for Config and Writer's CBuffer
	char *shm;

	// For connect/authenticate failures
	int reconnect_delay = 1;

	// This will be put into argv[0] (user convenience)
	char master_exec_name[CONFIG_OPTION_LENGTH] = "xc-master ";
	char wrapper_exec_name[CONFIG_OPTION_LENGTH] = "xc-wrapper ";

	// SIGHUP handler
	struct sigaction sighup_action;

	// Writer's config
	pthread_t writer;
	WriterConfig *writer_config = 0;
	// This sigset excludes SIGHUP so that actual signal comes into main thread only
	sigset_t writer_sigset;

	// Wrapper process exit status
	int wrapper_stat_loc;

	LINFO("xmcomp %d starting", VERSION);

	// Static Initialization
	sigemptyset(&writer_sigset);
	sigaddset(&writer_sigset, SIGHUP);

	// Install SIGHUP (reload config) handler
	memset(&sighup_action, 0, sizeof(sighup_action));
	sighup_action.sa_handler = reload_config;
	sigemptyset(&sighup_action.sa_mask);
	sigaction(SIGHUP, &sighup_action, 0);

	config = malloc(sizeof(*config));
	memset(config, 0, sizeof(config));
	// Some defaults
	config->recovery.stanza_size = 65536;
	config->reader.block = 0;
	config->reader.buffer = 1 << 20;
	config->reader.queue = 1024;
	config->writer.buffer = 1 << 20;
	config->parser.threads = 3;
	config->parser.buffer = 1 << 20;
	config_read(stdin, config);

	log_level = config->logger.level;

	strncat(master_exec_name, config->component.hostname, CONFIG_OPTION_LENGTH);
	strncat(wrapper_exec_name, config->component.hostname, CONFIG_OPTION_LENGTH);
	strcpy(argv[0], master_exec_name);

	shm = shm_alloc(config->component.hostname,
			sizeof(*config),
			sizeof(*writer_config),
			config->writer.buffer);
	memcpy(shm, config, sizeof(*config));
	free(config);
	config = (XmcompConfig *)shm;
	shm += sizeof(*config);

	writer_config = (WriterConfig *)(shm);
	shm += sizeof(*writer_config);

	cbuffer_init(&writer_config->cbuffer, shm, config->writer.buffer);

	while (1) {
		LINFO("connecting to %s:%d", config->network.host, config->network.port);

		writer_config->socket = net_connect(config->network.host, config->network.port);
		if (writer_config->socket >= 0) {
			LINFO("opening XMPP stream to %s", config->component.hostname);
			if (net_stream(writer_config->socket,
						"xmcomp",
						config->component.hostname,
						config->component.password)) {
				net_disconnect(writer_config->socket);
				writer_config->socket = -1;
			}
		}

		if (writer_config->socket < 0) {
			LERROR("retrying in %d second(s)", reconnect_delay);
			sleep(reconnect_delay);
			if (reconnect_delay < 60) {
				reconnect_delay <<= 1;
			}
			continue;
		}

		reconnect_delay = 1;

		writer_config->enabled = 1;
		LINFO("creating writer thread");
		pthread_create(&writer, 0, writer_thread_entry, (void *)writer_config);
		pthread_sigmask(SIG_BLOCK, &writer_sigset, 0);

		while (writer_config->enabled) {
			sleep(1);
			wrapper = fork();
			if (wrapper == -1) {
				LERRNO("cannot fork", errno);
			} else if (!wrapper) {
				// Child process
				strcpy(argv[0], wrapper_exec_name);
				return wrapper_main(config, writer_config);
			}
			while (wrapper != (pid_t)-1) {
				LINFO("waiting for child process PID %d", wrapper);
				wrapper = wait(&wrapper_stat_loc);
				if (wrapper == -1) {
					LWARN("signal received while waiting for child process");
				} else {
					if (wrapper != (pid_t)-1) {
						if (WIFEXITED(wrapper_stat_loc)) {
							LINFO("subprocess exited with code %d", WEXITSTATUS(wrapper_stat_loc));
						} else if (WIFSIGNALED(wrapper_stat_loc)) {
							LWARN("subprocess killed by signal %d", WTERMSIG(wrapper_stat_loc));
						} else if (WIFSTOPPED(wrapper_stat_loc)) {
							LWARN("subprocess stopped by signal %d; waiting", WSTOPSIG(wrapper_stat_loc));
						} else if (WIFCONTINUED(wrapper_stat_loc)) {
							LWARN("subprocess continued; waiting");
						}
					}
				}
			}
		}

		LINFO("joining writer thread");
		pthread_join(writer, 0);

		LINFO("clearing writer buffer and disconnecting");
		cbuffer_clear(&writer_config->cbuffer);
		net_unstream(writer_config->socket);
		net_disconnect(writer_config->socket);
	}

	return 0;
}
