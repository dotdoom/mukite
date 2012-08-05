#ifndef CONFIG_H
#define CONFIG_H

#include "xmcomp/network.h"
#include "xmcomp/reader.h"
#include "xmcomp/writer.h"

#include "rooms.h"
#include "parser.h"

#define PARSERS_COUNT_LIMIT 1024

#define CONFIG_OPTION_LENGTH 1024

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
		char data_file[CONFIG_OPTION_LENGTH];
	} parser;

	struct {
		int level;
	} logger;

	Socket socket;

	pthread_t writer_thread_id;
	WriterConfig writer_thread;

	pthread_t reader_thread_id;
	ReaderConfig reader_thread;

	ParserConfig parser_threads[PARSERS_COUNT_LIMIT];
	int parser_threads_count;
	
	Rooms rooms;
} Config;

void config_init(Config *, char *);
BOOL config_read(Config *);
void config_apply(Config *);

#endif
