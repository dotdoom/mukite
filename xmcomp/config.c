#include <string.h>

#include "logger.h"

#include "config.h"

#define READ_CONFIG_STR(name, type) \
	if (!strcmp(cfg_opt_name, #name)) { \
		fscanf(file, "%1023s", cfg_opt_str); \
		if ((config->last_change_type & type) != type) { \
			if (strcmp(cfg_opt_str, config->name)) { \
				config->last_change_type = type; \
			} \
		} \
		strcpy(config->name, cfg_opt_str); \
	} else

#define READ_CONFIG_INT(name, type) \
	if (!strcmp(cfg_opt_name, #name)) { \
		fscanf(file, "%d", &cfg_opt_int); \
		if ((config->last_change_type & type) != type) { \
			if (cfg_opt_int != config->name) { \
				config->last_change_type |= type; \
			} \
		} \
		config->name = cfg_opt_int; \
	} else

BOOL config_read(XmcompConfig *config) {
	char cfg_opt_name[CONFIG_OPTION_LENGTH],
		 cfg_opt_str[CONFIG_OPTION_LENGTH];
	int cfg_opt_int, error;
	FILE *file = 0;

	config->last_change_type = UCCA_NONE;

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

		READ_CONFIG_STR(network.host, UCCA_RECONNECT)
		READ_CONFIG_INT(network.port, UCCA_RECONNECT)

		READ_CONFIG_STR(component.password, UCCA_RECONNECT)
		// XXX(artem): another way to ignore new hostname
		if (!strcmp(cfg_opt_name, "component.hostname")) {
			if (config->component.hostname[0]) {
				// hostname already set, skip new value
				fscanf(file, "%*s");
			} else {
				READ_CONFIG_STR(component.hostname, UCCA_RECONNECT);
			}
		} else

		READ_CONFIG_INT(reader.buffer, UCCA_NO_RESTART)
		READ_CONFIG_INT(reader.block, UCCA_NO_RESTART)
		READ_CONFIG_INT(reader.queue, UCCA_RESTART_READER)
		READ_CONFIG_INT(reader.max_stanza_size, UCCA_NO_RESTART)

		READ_CONFIG_INT(writer.buffer, UCCA_RESTART_WRITER)

		READ_CONFIG_STR(parser.library, UCCA_RELOAD_LIBRARY)
		READ_CONFIG_STR(parser.data_file, UCCA_NOTIFY_LIBRARY)
		READ_CONFIG_STR(parser.config_file, UCCA_NOTIFY_LIBRARY)
		READ_CONFIG_INT(parser.threads, UCCA_NOTIFY_LIBRARY)
		READ_CONFIG_INT(parser.buffer, UCCA_NOTIFY_LIBRARY)

		READ_CONFIG_INT(logger.level, UCCA_NO_RESTART)

		LWARN("unknown config option '%s'", cfg_opt_name);
	}

	if (file != stdin) {
		fclose(file);
	}
	return TRUE;
}
