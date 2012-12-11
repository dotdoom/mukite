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

		READ_CONFIG_STR(parser.data_file)
		READ_CONFIG_INT(parser.threads)
		READ_CONFIG_INT(parser.buffer)

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
	config->parser.threads = 3;
	config->parser.buffer = 1 << 20;
	rooms_init(&config->rooms);
}

void config_destroy(Config *config) {
	rooms_destroy(&config->rooms);
	acl_destroy(&config->acl_config);
}

void config_apply(Config *config) {
	int i, error;
	ParserConfig *parser = 0;
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

	if (config->parser.threads > PARSERS_COUNT_LIMIT) {
		LERROR("%d exceeds parsers limit %d, shrinking",
				config->parser.threads, PARSERS_COUNT_LIMIT);
		config->parser.threads = PARSERS_COUNT_LIMIT;
	}

	if (config->parser_threads.count < config->parser.threads) {
		for (i = config->parser_threads.count; i < config->parser.threads; ++i) {
			parser = &config->parser_threads.threads[i];
			parser->enabled = TRUE;
			parser->global_config = config;
			pthread_create(&parser->thread, 0, parser_thread_entry, (void *)parser);
			++config->parser_threads.count;
		}
	} else if (config->parser_threads.count > config->parser.threads) {
		for (i = config->parser.threads; i < config->parser_threads.count; ++i) {
			parser = &config->parser_threads.threads[i];
			parser->enabled = FALSE;
			--config->parser_threads.count;
		}
	}
}
