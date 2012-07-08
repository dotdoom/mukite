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
#include "sighelper.h"

XmcompConfig *config = 0;
pid_t wrapper = 0;

static void reload_config(int signal) {
	config_read(stdin, config);

	log_level = config->logger.level;

	if ((config->last_change_type & UCCA_RECONNECT) == UCCA_RECONNECT) {
		LWARN("some configuration changes will only be applied after connection restart");
	}
	if ((config->last_change_type & UCCA_RESTART_WRITER) == UCCA_RESTART_WRITER) {
		LWARN("some configuration changes will only be applied after master restart");
	}

	if (wrapper) {
		kill(wrapper, SIGHUP);
	}
}

char *shm_alloc(char *name_postfix, int size) {
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

	// Writer's config
	pthread_t writer;

	// Wrapper process exit status
	int wrapper_stat_loc;
	pid_t wrapper_wait_res;

	LINFO("xmcomp %d starting", VERSION);

	sighelper_sigaction(SIGHUP, reload_config);

	config = malloc(sizeof(*config));
	memset(config, 0, sizeof(config));
	// Some defaults
	config->reader.max_stanza_size = 65536;
	config->reader.block = 0;
	config->reader.buffer = 1 << 20;
	config->reader.queue = 1024;
	config->writer.buffer = 1 << 20;
	config->parser.threads = 3;
	config->parser.buffer = 1 << 20;
	reload_config(0);

	strncat(master_exec_name, config->component.hostname, CONFIG_OPTION_LENGTH);
	strncat(wrapper_exec_name, config->component.hostname, CONFIG_OPTION_LENGTH);
	strcpy(argv[0], master_exec_name);

	shm = shm_alloc(config->component.hostname,
			sizeof(*config) +
			config->writer.buffer);
	memcpy(shm, config, sizeof(*config));
	free(config);
	config = (XmcompConfig *)shm;
	shm += sizeof(*config);

	cbuffer_init(&config->writer_thread.cbuffer, shm, config->writer.buffer);

	while (1) {
		LINFO("connecting to %s:%d", config->network.host, config->network.port);

		config->writer_thread.socket = net_connect(config->network.host, config->network.port);
		if (config->writer_thread.socket >= 0) {
			LINFO("opening XMPP stream to %s", config->component.hostname);
			if (net_stream(config->writer_thread.socket,
						"xmcomp",
						config->component.hostname,
						config->component.password)) {
				net_disconnect(config->writer_thread.socket);
				config->writer_thread.socket = -1;
			}
		}

		if (config->writer_thread.socket < 0) {
			LERROR("retrying in %d second(s)", reconnect_delay);
			sleep(reconnect_delay);
			if (reconnect_delay < 60) {
				reconnect_delay <<= 1;
			}
			continue;
		}

		reconnect_delay = 1;

		config->writer_thread.enabled = 1;
		LINFO("creating writer thread");
		pthread_create(&writer, 0, writer_thread_entry, (void *)&config->writer_thread);

		config->reader.recovery_mode = 0;
		while (config->writer_thread.enabled) {
			sleep(1);
			wrapper = fork();
			if (wrapper == -1) {
				LERRNO("cannot fork", errno);
			} else if (!wrapper) {
				// Child process
				strcpy(argv[0], wrapper_exec_name);
				return wrapper_main(config);
			}
			LINFO("waiting for subprocess, PID %d", wrapper);
			while (wrapper) {
				wrapper_wait_res = wait(&wrapper_stat_loc);
				if (wrapper_wait_res == -1 || wrapper_wait_res == (pid_t)-1) {
					if (errno == EINTR) {
						LDEBUG("wait() interrupted by signal, resuming");
					} else {
						LERRNO("wrapper process exited magically or too fast, wait() says", errno);
						wrapper = 0;
					}
				} else {
					if (WIFEXITED(wrapper_stat_loc)) {
						LINFO("PID %d exited with code %d",
								wrapper, WEXITSTATUS(wrapper_stat_loc));
						wrapper = 0;
					} else if (WIFSIGNALED(wrapper_stat_loc)) {
						LWARN("PID %d killed by signal %d",
								wrapper, WTERMSIG(wrapper_stat_loc));
						wrapper = 0;
					} else if (WIFSTOPPED(wrapper_stat_loc)) {
						LWARN("PID %d stopped by signal %d; waiting",
								wrapper, WSTOPSIG(wrapper_stat_loc));
					} else if (WIFCONTINUED(wrapper_stat_loc)) {
						LWARN("PID %d continued; waiting", wrapper);
					}
				}
			}

			// When wrapper dies we always switch to Recovery Mode
			config->reader.recovery_mode = 1;
		}

		LINFO("joining writer thread");
		pthread_join(writer, 0);

		LINFO("clearing writer buffer and disconnecting");
		cbuffer_clear(&config->writer_thread.cbuffer);
		net_unstream(config->writer_thread.socket);
		net_disconnect(config->writer_thread.socket);
	}

	return 0;
}
