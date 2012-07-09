#include <pthread.h>
#include <string.h>
#include "xmcomp/common.h"
#include "xmcomp/queue.h"
#include "xmcomp/cbuffer.h"
#include "xmcomp/config.h"

#include "config.h"
#include "parser.h"

void __attribute__ ((constructor)) mu_component_init(void) {
}

void __attribute__ ((destructor)) mu_component_fini(void) {
}

void set_thread_count(MukiteConfig *config, int count) {
	// FIXME(artem): make this function thread-safe
	int i;
	ParserConfig *pc;
	StanzaEntry *fake_stanza;

	LINFO("changing parsers count: %d -> %d", config->parsers_count, count);
	if (count > PARSERS_COUNT_LIMIT) {
		LERROR("attempt to exceed parsers limit %d, shrinking counter",
				count = PARSERS_COUNT_LIMIT);
	}

	if (count < config->parsers_count) {
		for (i = count; i < config->parsers_count; ++i) {
			config->parsers[i].enabled = FALSE;
		}
		// Fake some empty stanza buffers
		for (i = count; i < config->parsers_count; ++i) {
			fake_stanza = queue_pop_free(&config->xc_config->reader_thread.queue);
			fake_stanza->data_size = 0;
			queue_push_data(&config->xc_config->reader_thread.queue, fake_stanza);
		}
		for (i = count; i < config->parsers_count; ++i) {
			pthread_join(config->parsers[i].thread, 0);
		}
	} else {
		for (i = config->parsers_count; i < count; ++i) {
			pc = &config->parsers[i];
			pc->enabled = TRUE;
			pc->global_config = config;
			pthread_create(&pc->thread, 0, parser_thread_entry, (void *)pc);
		}
	}
	config->parsers_count = count;
}

int start(void *void_config) {
	MukiteConfig *config = (MukiteConfig *)void_config;
	set_thread_count(config, config->xc_config->parser.threads);
	return 1;
}

void stop(void *void_config) {
	set_thread_count((MukiteConfig *)void_config, 0);
}

void reconfigure(void *void_config) {
	MukiteConfig *config = (MukiteConfig*)config;
	if (config->xc_config->parser.threads != config->parsers_count) {
		set_thread_count(config, config->xc_config->parser.threads);
	}
}

void *initialize(XmcompConfig *component_config) {
	MukiteConfig *config;

	config = malloc(sizeof(*config));
	memset(config, 0, sizeof(*config));
	config->xc_config = component_config;
	rooms_init(&config->rooms);

	return config;
}

void finalize(void *void_config) {
	MukiteConfig *config = (MukiteConfig *)void_config;
	// TODO(artem): here
	free(config);
}
