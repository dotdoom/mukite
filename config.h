#ifndef CONFIG_H
#define CONFIG_H

#include "xmcomp/network.h"
#include "xmcomp/reader.h"
#include "xmcomp/writer.h"

#include "rooms.h"
#include "worker.h"
#include "acl.h"

#define WORKERS_COUNT_LIMIT 1024

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
	} worker;

	struct {
		char data_file[CONFIG_OPTION_LENGTH];
		int default_role;
	} acl;

	struct {
		int level;
	} logger;

	Socket socket;

	WriterConfig writer_thread;
	ReaderConfig reader_thread;

	struct {
		int count;
		WorkerConfig threads[WORKERS_COUNT_LIMIT];
	} worker_threads;

	Rooms rooms;
	ACLConfig acl_config;
	time_t startup;
} Config;

void config_init(Config *, char *filename);
void config_destroy(Config *);
BOOL config_read(Config *);
void config_apply(Config *);

#endif
