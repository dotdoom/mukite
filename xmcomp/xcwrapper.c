#include <dlfcn.h>
#include <stdlib.h>

#include "reader.h"
#include "logger.h"

#include "xcwrapper.h"

typedef void (*ParserStart)(XmcompConfig *, StanzaQueue *, CBuffer *);
typedef void (*ParserStop)(void);

int wrapper_main(XmcompConfig *config, WriterConfig *writer_config) {
	void *module;
	pthread_t reader;
	ReaderConfig reader_config;
	StartParser *start_parser;
	StopParser *stop_parser;
	char *error;

	LINFO("loading library %s", config->parser.library);
	module = dlopen(config->parser.library, RTLD_LAZY);
	if (!module) {
		LFATAL("cannot open library %s: %s", config->parser.library, dlerror());
	}

	start_parser = dlsym(module, "start_parser");
	if (!(error = dlerror())) {
		stop_parser = dlsym(module, "stop_parser");
	}
	if ((error = dlerror())) {
		LFATAL("cannot find function in the library %s: %s",
				config->parser.library,
				error);
	}

	reader_config.socket = writer_config->socket;
	queue_init(&reader_config.queue, config->reader.queue);
	reader_config.enabled = 1;

	LINFO("starting reader thread");
	pthread_create(&reader, 0, reader_thread_entry, (void *)&reader_config);

	LINFO("starting component library");
	(*start_parser)(config, &reader_config.queue, &writer_config->cbuffer);

    pthread_join(reader, 0);
	LINFO("reader exited, stopping component library");
	(*stop_parser)();

	dlclose(module);
	return 0;
}
