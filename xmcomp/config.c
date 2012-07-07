#include <string.h>

#include "logger.h"

#include "config.h"

#define READ_CONFIG_STR(name, level) \
	if (!strcmp(cfg_opt_name, #name)) { \
		fscanf(file, "%1023s", cfg_opt_str); \
		if (!(cfg_change_type & CONFIG_CHANGE_ ## level)) { \
			if (strcmp(cfg_opt_str, config->name)) { \
				cfg_change_type = CONFIG_CHANGE_ ## level; \
			} \
		} \
		strcpy(config->name, cfg_opt_str); \
	} else

#define READ_CONFIG_INT(name, level) \
	if (!strcmp(cfg_opt_name, #name)) { \
		fscanf(file, "%d", &cfg_opt_int); \
		if (!(cfg_change_type & CONFIG_CHANGE_ ## level)) { \
			if (cfg_opt_int != config->name) { \
				cfg_change_type |= CONFIG_CHANGE_ ## level; \
			} \
		} \
		config->name = cfg_opt_int; \
	} else

int config_read(FILE *file, XmcompConfig *config, char allow_hostname_change) {
	char cfg_opt_name[CONFIG_OPTION_LENGTH],
		 cfg_opt_str[CONFIG_OPTION_LENGTH];
	int cfg_opt_int, cfg_change_type = CONFIG_CHANGE_NONE;

	rewind(file);
	while (fscanf(file, "%1023s", cfg_opt_name) == 1) {
		if (cfg_opt_name[0] == '#') {
			// Skip comments
			scanf("%*[^\n]\n");
			continue;
		}

		READ_CONFIG_STR(network.host, RECONNECT)
		READ_CONFIG_INT(network.port, RECONNECT)

		READ_CONFIG_STR(component.password, RECONNECT)
		// XXX(artem): we should find a better way for hostname handling
		if (allow_hostname_change && !strcmp(cfg_opt_name, "component.hostname")) {
			READ_CONFIG_STR(component.hostname, RECONNECT)
		}

		READ_CONFIG_INT(reader.buffer, NO_RESTART)
		READ_CONFIG_INT(reader.block, NO_RESTART)
		READ_CONFIG_INT(reader.queue, RESTART_READER)
		READ_CONFIG_INT(reader.max_stanza_size, NO_RESTART)

		READ_CONFIG_INT(writer.buffer, RESTART_WRITER)

		READ_CONFIG_STR(parser.library, RELOAD_LIBRARY)
		READ_CONFIG_STR(parser.data_file, NOTIFY_LIBRARY)
		READ_CONFIG_STR(parser.config_file, NOTIFY_LIBRARY)
		READ_CONFIG_INT(parser.threads, NOTIFY_LIBRARY)
		READ_CONFIG_INT(parser.buffer, NOTIFY_LIBRARY)

		READ_CONFIG_INT(logger.level, NO_RESTART)

		LWARN("unknown option %s", cfg_opt_name);
	}

	return cfg_change_type;
}
