#include <pthread.h>

#include "xmcomp/common.h"
#include "xmcomp/queue.h"
#include "xmcomp/cbuffer.h"
#include "xmcomp/config.h"
#include "parser.h"

void __attribute__ ((constructor)) mu_component_init(void) {
	LINFO("initialized");
}

void __attribute__ ((destructor)) mu_component_fini(void) {
	LINFO("finalized");
}

void start_parser(XmcompConfig *config, StanzaQueue *stanza_queue, CBuffer *output_buffer) {
/*	pthread_attr_t attr;
	int i;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	config->parsers = malloc(config->parsers_limit * sizeof(*config->parsers));
	config->parsers_count = 0;
	LINFO("starting %d threads", config->parsers_limit);
	for (i = 0; i < config->parsers_limit; ++i) {
		pthread_create(&config->parsers[i], &attr, parser_thread_entry, (void *)config);
		++config->parsers_count;
	}
	LINFO("created %d threads", config->parsers_count);*/
}

void stop_parser() {
}

int save_dump(FILE *f) {
	return 0;
}

int load_dump(FILE *f) {
	return 0;
}
