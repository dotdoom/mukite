#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

#include "reader.h"
#include "logger.h"

#include "xcwrapper.h"

typedef void (*ReconfigureFunction)(XmcompConfig *, StanzaQueue *, CBuffer *);
typedef int (*StartFunction)();
typedef void (*StopFunction)();

void *dlsym_m(void *handle, char *symbol) {
	void *ptr = 0;
	char *error = 0;
	ptr = dlsym(handle, symbol);
	error = dlerror();
	if (!ptr || error) {
		LFATAL("cannot find function '%s': %s", symbol, error);
	}
	return ptr;
}

int wrapper_main(XmcompConfig *config, WriterConfig *writer_config) {
	void *module = 0;
	pthread_t reader;
	ReaderConfig reader_config;

	ReconfigureFunction lib_reconfigure = 0;
	StartFunction lib_start = 0;
	StopFunction lib_stop = 0;

	LDEBUG("loading library %s", config->parser.library);
	module = dlopen(config->parser.library, RTLD_LAZY);
	if (!module) {
		LFATAL("cannot open library %s: %s", config->parser.library, dlerror());
	}

	lib_start = dlsym_m(module, "start");
	lib_reconfigure = dlsym_m(module, "reconfigure");
	lib_stop = dlsym_m(module, "stop");

	reader_config.socket = writer_config->socket;
	queue_init(&reader_config.queue, config->reader.queue);
	reader_config.enabled = 1;

	LINFO("applying library starting configuration");
	(*lib_reconfigure)(config, &reader_config.queue, &writer_config->cbuffer);

	LINFO("starting component library");
	if ((*lib_start)()) {
		LINFO("starting reader thread");
		pthread_create(&reader, 0, reader_thread_entry, (void *)&reader_config);

	    pthread_join(reader, 0);
		LINFO("reader exited, stopping component library");
		(*lib_stop)();
	} else {
		LERROR("component library start routine reports failure, exiting");
	}

	dlclose(module);
	return 0;
}
