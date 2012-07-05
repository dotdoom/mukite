#ifndef RCONFIG_H
#define RCONFIG_H

#include "queue.h"
#include "cbuffer.h"
#include "rooms.h"
#include "common.h"

#define RCF_READER 1
#define RCF_WRITER 2
#define RCF_PARSER 4
#define RCF_RECOVERY 8

typedef struct {
	StanzaQueue recv_queue;
	CBuffer send_cbuffer;

	int socket;
	int flags;
	int recovery_stanza_size;

	int parsers_limit, parsers_count, parser_output_buffer_size;
	pthread_t *parsers;

	Rooms rooms;

	Buffer hostname;

	char reserved[1024];
} RuntimeConfig;

#endif
