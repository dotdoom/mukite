#ifndef XMCOMP_CONFIG_H
#define XMCOMP_CONFIG_H

#include "reader.h"
#include "writer.h"

#include <stdio.h>
#define CONFIG_OPTION_LENGTH 1024

typedef struct {
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
		int max_stanza_size;
	} reader;
	struct {
		int buffer;
	} writer;
	struct {
		char library[CONFIG_OPTION_LENGTH];
		char config_file[CONFIG_OPTION_LENGTH];
		char data_file[CONFIG_OPTION_LENGTH];
		int threads;
		int buffer;
	} parser;

	struct {
		int level;
	} logger;
} XmcompConfig;

// Actions to take, depending on config changes:
#define CONFIG_CHANGE_NONE 0
// There are changes which can be applied immediately, w/o any restarts
#define CONFIG_CHANGE_NO_RESTART 1
// There are changes that require component library reloading (typically filename change)
#define CONFIG_CHANGE_RELOAD_LIBRARY 2
// There are changes that component library may be interested in, and handle as appropriate (typically parser config)
#define CONFIG_CHANGE_NOTIFY_LIBRARY 4
// There are changes that require wrapper process restart (reader hard changes)
#define CONFIG_CHANGE_RESTART_READER 8
// There are changes that require writer restart
#define CONFIG_CHANGE_RESTART_WRITER 16
// There are changes requiring connection reset (reauth or host/port change)
#define CONFIG_CHANGE_RECONNECT 32
int config_read(FILE *, XmcompConfig *);

#endif
