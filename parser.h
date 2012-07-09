#ifndef PARSER_H
#define PARSER_H

#include <pthread.h>

#include "xmcomp/common.h"

typedef struct {
	BOOL enabled;
	pthread_t thread;
	void *global_config;
} ParserConfig;

void *parser_thread_entry(void *void_parser_config);

#endif
