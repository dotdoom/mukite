#include <errno.h>
#include <string.h>

#include "xmcomp/logger.h"

#include "config.h"

#define MAX_ACLS 10240

#define READ_CONFIG(name, type, prefix) \
	if (!strcmp(cfg_opt_name, #name)) { \
		fscanf(file, type, prefix config->name); \
	} else

#define READ_CONFIG_STR(name) \
	READ_CONFIG(name, "%1023s", )

#define READ_CONFIG_INT(name) \
	READ_CONFIG(name, "%d", &)

BOOL config_read(Config *config) {
	char cfg_opt_name[CONFIG_OPTION_LENGTH];
	int error;
	FILE *file = 0;

	LINFO("loading configuration from '%s'", config->filename[0] ? config->filename : "stdin");
	if (config->filename[0]) {
		file = fopen(config->filename, "r");
		if (!file) {
			error = errno;
			LERRNO("could not open configuration file %s for reading",
					error, config->filename);
			return FALSE;
		}
	} else {
		file = stdin;
		rewind(file);
	}

	while (fscanf(file, "%1023s", cfg_opt_name) == 1) {
		if (cfg_opt_name[0] == '#') {
			// Skip comments
			fscanf(file, "%*[^\n]\n");
			continue;
		}

		READ_CONFIG_STR(network.host)
		READ_CONFIG_INT(network.port)

		READ_CONFIG_STR(component.password)
		READ_CONFIG_STR(component.hostname)

		READ_CONFIG_INT(reader.buffer)
		READ_CONFIG_INT(reader.block)
		READ_CONFIG_INT(reader.queue)

		READ_CONFIG_INT(writer.buffer)

		READ_CONFIG_STR(worker.data_file)
		READ_CONFIG_INT(worker.threads)
		READ_CONFIG_INT(worker.buffer)
		READ_CONFIG_INT(worker.deciseconds_limit)

		READ_CONFIG_STR(acl.data_file)
		READ_CONFIG_INT(acl.default_role)

		READ_CONFIG_INT(logger.level)

		LWARN("unknown config option '%s'", cfg_opt_name);
	}

	if (file != stdin) {
		fclose(file);
	}
	return TRUE;
}

void config_init(Config *config, char *filename) {
	memset(config, 0, sizeof(*config));
	if (filename) {
		strncpy(config->filename, filename, FILENAME_MAX);
	}
	acl_init(&config->acl_config);

	// Some defaults
	config->reader.block = 0;
	config->reader.buffer = 1 << 20;
	config->reader.queue = 1024;
	config->writer.buffer = 1 << 20;
	config->worker.threads = 3;
	config->worker.buffer = 1 << 20;
	config->worker.deciseconds_limit = 2;
	rooms_init(&config->rooms);
	uname(&config->uname);
}

void config_destroy(Config *config) {
	rooms_destroy(&config->rooms);
	acl_destroy(&config->acl_config);
}

void config_apply(Config *config) {
	int i, error;
	WorkerConfig *worker = 0;
	FILE *acl_data_file = 0;

	LDEBUG("applying configuration settings");

	log_level = config->logger.level;

	config->reader_thread.queue.fixed_block_buffer_size =
		config->reader.block;
	config->reader_thread.queue.network_buffer_size =
		config->reader.buffer;
	config->acl_config.default_role =
		config->acl.default_role;

	if (!(acl_data_file = fopen(config->acl.data_file, "r"))) {
		error = errno;
		LERRNO("could not open acl data file '%s' for reading", error, config->acl.data_file);
	} else {
		acl_deserialize(&config->acl_config, acl_data_file, MAX_ACLS);
		fclose(acl_data_file);
	}

	if (config->worker.threads > WORKERS_COUNT_LIMIT) {
		LERROR("%d exceeds workers limit %d, shrinking",
				config->worker.threads, WORKERS_COUNT_LIMIT);
		config->worker.threads = WORKERS_COUNT_LIMIT;
	}

	if (config->worker_threads.count < config->worker.threads) {
		for (i = config->worker_threads.count; i < config->worker.threads; ++i) {
			worker = &config->worker_threads.threads[i];
			worker->enabled = TRUE;
			worker->global_config = config;
			pthread_create(&worker->thread, 0, worker_thread_entry, (void *)worker);
			++config->worker_threads.count;
		}
	} else if (config->worker_threads.count > config->worker.threads) {
		for (i = config->worker.threads; i < config->worker_threads.count; ++i) {
			worker = &config->worker_threads.threads[i];
			worker->enabled = FALSE;
			--config->worker_threads.count;
		}
	}
}
