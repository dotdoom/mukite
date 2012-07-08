#ifndef XMCOMP_CONFIG_H
#define XMCOMP_CONFIG_H

#include <stdio.h>

#include "reader.h"
#include "writer.h"

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
		char recovery_mode;
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

	int last_change_type;

	WriterConfig writer_thread;
	ReaderConfig reader_thread;
} XmcompConfig;

#define UCC_ACTION (1 << 8)

// User-Config-Change-Actions to take:
#define UCCA_NONE (UCC_ACTION * 0)
// There are changes which can be applied immediately, w/o any restarts
#define UCCA_NO_RESTART (UCC_ACTION * 1)
// There are changes that require component library reloading (typically filename change)
#define UCCA_RELOAD_LIBRARY (UCC_ACTION * 2)
// There are changes that component library may be interested in, and handle as appropriate (typically parser config)
#define UCCA_NOTIFY_LIBRARY (UCC_ACTION * 4)
// There are changes that require wrapper process restart (reader hard changes)
#define UCCA_RESTART_READER (UCC_ACTION * 8)
// There are changes that require writer restart
#define UCCA_RESTART_WRITER (UCC_ACTION * 16)
// There are changes requiring connection reset (reauth or host/port change)
#define UCCA_RECONNECT (UCC_ACTION * 32)

void config_read(FILE *, XmcompConfig *);

#endif
