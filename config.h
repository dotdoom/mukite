#ifndef CONFIG_H
#define CONFIG_H

#include "xmcomp/config.h"

#include "rooms.h"
#include "parser.h"

#define PARSERS_COUNT_LIMIT 1024

typedef struct {
	XmcompConfig *xc_config;

	Rooms rooms;

	int parsers_count;
	ParserConfig parsers[PARSERS_COUNT_LIMIT];
} MukiteConfig;

#endif
