#ifndef CONFIG_H
#define CONFIG_H

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

#define CONFIG_STATIC 0
#define CONFIG_LIBRARY_CHANGED 1
#define CONFIG_WRAPPER_CHANGED 2
#define CONFIG_MASTER_CHANGED 3
int config_read(FILE *, XmcompConfig *);
void config_write(FILE *, XmcompConfig *);

void config_apply_to_master(XmcompConfig *, WriterConfig *);
void config_apply_to_wrapper(XmcompConfig *, ReaderConfig *);

#endif
