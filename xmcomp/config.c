#include <string.h>

#include "logger.h"

#include "config.h"

#define READ_CONFIG(name, type, prefix) \
	if (!strcmp(cfg_opt_name, #name)) { \
		sscanf(cfg_opt_value, "%" type, prefix config->name); \
	} else

#define READ_CONFIG_INT(name) \
	READ_CONFIG(name, "d", &)

#define READ_CONFIG_STR(name) \
	READ_CONFIG(name, "1023s", )

void config_read(FILE *file, XmcompConfig *config) {
	char cfg_opt_name[CONFIG_OPTION_LENGTH], cfg_opt_value[CONFIG_OPTION_LENGTH];

	rewind(file);

	while (fscanf(file, "%30s %100s", cfg_opt_name, cfg_opt_value) == 2) {
		READ_CONFIG_STR(network.host)
		READ_CONFIG_INT(network.port)

		READ_CONFIG_STR(component.password)
		READ_CONFIG_STR(component.hostname)

		READ_CONFIG_INT(recovery.stanza_size)

		READ_CONFIG_INT(reader.buffer)
		READ_CONFIG_INT(reader.block)
		READ_CONFIG_INT(reader.queue)

		READ_CONFIG_INT(writer.buffer)

		READ_CONFIG_STR(parser.library)
		READ_CONFIG_STR(parser.data_file)
		READ_CONFIG_STR(parser.config_file)
		READ_CONFIG_INT(parser.threads)
		READ_CONFIG_INT(parser.buffer)

		READ_CONFIG_INT(logger.level)

		LWARN("unknown option %s", cfg_opt_name);
	}
}
