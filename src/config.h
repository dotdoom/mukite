#ifndef CONFIG_H
#define CONFIG_H

#define CONFIG_OPTION_LENGTH 1024

#include "xmcomp/src/common.h"

typedef struct {
	char filename[FILENAME_MAX];

	struct {
		char host[CONFIG_OPTION_LENGTH];
		int  port;
	} network;
	struct {
		char password[CONFIG_OPTION_LENGTH];
		char hostname[CONFIG_OPTION_LENGTH];
	} component;

	struct {
		int block;
		int buffer;
		int queue;
	} reader;
	struct {
		int buffer;
	} writer;
	struct {
		int threads;
		int buffer;
		int deciseconds_limit;
		char data_file[CONFIG_OPTION_LENGTH];
	} worker;

	struct {
		char data_file[CONFIG_OPTION_LENGTH];
		int default_role;
	} acl;

	struct {
		int level;
	} logger;
} Config;

void config_init(Config *, char *filename);
BOOL config_read(Config *);

#endif
