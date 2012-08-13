#include <errno.h>
#include <string.h>

#include "xmcomp/logger.h"

#include "config.h"

#define ACTION_NONE
#define ACTION_RESTART

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

	// Some defaults
	config->reader.block = 0;
	config->reader.buffer = 1 << 20;
	config->reader.queue = 1024;
	config->writer.buffer = 1 << 20;
	config->parser.threads = 3;
	config->parser.buffer = 1 << 20;
	rooms_init(&config->rooms);
}

void config_apply(Config *config) {
	int i;
	ParserConfig *parser = 0;

	log_level = config->logger.level;

	config->reader_thread.queue.fixed_block_buffer_size =
		config->reader.block;
	config->reader_thread.queue.network_buffer_size =
		config->reader.buffer;
	config->acl_config.default_role =
		config->acl.default_role;

	if (config->parser.threads > PARSERS_COUNT_LIMIT) {
		LERROR("%d exceeds parsers limit %d, shrinking",
				config->parser.threads, PARSERS_COUNT_LIMIT);
		config->parser.threads = PARSERS_COUNT_LIMIT;
	}

	if (config->parser_threads_count < config->parser.threads) {
		for (i = config->parser_threads_count; i < config->parser.threads; ++i) {
			parser = &config->parser_threads[i];
			parser->enabled = TRUE;
			parser->global_config = config;
			pthread_create(&parser->thread, 0, parser_thread_entry, (void *)parser);
			++config->parser_threads_count;
		}
	} else if (config->parser_threads_count > config->parser.threads) {
		for (i = config->parser.threads; i < config->parser_threads_count; ++i) {
			config->parser_threads[i].enabled = FALSE;
			// TODO(artem): join parser thread
			--config->parser_threads_count;
		}
	}
}
