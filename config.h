#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
	StanzaQueue recv_queue;
	CBuffer send_cbuffer;

	int parsers_limit, parsers_count, parser_output_buffer_size;
	pthread_t *parsers;

	Rooms rooms;

	Buffer hostname;

	char reserved[1024];
} RuntimeConfig;

#endif
