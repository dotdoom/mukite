#ifndef CONFIG_H
#define CONFIG_H

#include <sys/utsname.h>

#include "xmcomp/network.h"
#include "xmcomp/reader.h"
#include "xmcomp/writer.h"

#include "rooms.h"
#include "worker.h"
#include "acl.h"
#include "timer.h"

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

	Socket socket;

	WriterConfig writer_thread;
	ReaderConfig reader_thread;

	struct {
		int count;
		WorkerConfig threads[WORKERS_COUNT_LIMIT];
	} worker_threads;

	TimerConfig timer_thread;

	Rooms rooms;
	ACLConfig acl_config;
	struct utsname uname;
} Config;

extern Config config;

void config_init(char *filename);
void config_destroy();
BOOL config_read();
void config_apply();

#endif
