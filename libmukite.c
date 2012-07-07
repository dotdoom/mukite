#include <pthread.h>

#include "xmcomp/common.h"
#include "xmcomp/queue.h"
#include "xmcomp/cbuffer.h"
#include "xmcomp/config.h"
#include "config.h"

// Personal config:
//  * parser statuses
//  * queue, cbuffer

//#include "parser.h"

struct {
	pthread_t parsers[1024];
	int parsers_count;

	XmcompConfig *component_config;
	StanzaQueue *stanza_queue;
	CBuffer *output_buffer;
} config;

void __attribute__ ((constructor)) mu_component_init(void) {
	config.parsers_count = 0;
	config.config = 0;
	config.stanza_queue = 0;
	config.output_buffer = 0;
}

void __attribute__ ((destructor)) mu_component_fini(void) {
	LDEBUG("finalized");
}

int start() {
	LINFO("starting %d threads", config->parser.threads);
	while (config.parsers_count < config->parser.threads) {
		pthread_create(&config.parsers[config.parsers_count],
				0, parser_thread_entry, (void *)&config);
		++config.parsers_count;
	}
}

void stop() {
	LINFO("stopping parsers");
	LINFO("stopped");
}

void reconfigure(XmcompConfig *component_config, StanzaQueue *stanza_queue, CBuffer *output_buffer) {
	config.config = component_config;
	config.stanza_queue = stanza_queue;
	config.output_buffer = output_buffer;
}
