#include <errno.h>
#include <string.h>

#include "xmcomp/logger.h"

#include "config.h"

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

	// Some defaults
	config->reader.buffer = 1 << 20;
	config->reader.queue = 1024;
	config->writer.buffer = 1 << 20;
	config->worker.threads = 3;
	config->worker.buffer = 1 << 20;
	config->worker.deciseconds_limit = 2;
}
