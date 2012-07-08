#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#include "reader.h"
#include "logger.h"
#include "sighelper.h"

#include "xcwrapper.h"

typedef void (*ConfigureFunction)(XmcompConfig *);
typedef int (*StartFunction)();
typedef void (*StopFunction)();
typedef void (*ReconfigureFunction)();

struct LibraryEntry {
	void *module;
	ConfigureFunction configure;
	StartFunction start;
	StopFunction stop;
	ReconfigureFunction reconfigure;
} library;

XmcompConfig *config;

static void *dlsym_m(void *handle, char *symbol) {
	void *ptr = 0;
	char *error = 0;
	ptr = dlsym(handle, symbol);
	error = dlerror();
	if (!ptr || error) {
		LERROR("cannot find function '%s': %s", symbol, error);
		return 0;
	}
	return ptr;
}

static int push_library() {
	LINFO("(re)loading component library");

	struct LibraryEntry new_lib;

	new_lib.module = dlopen(config->parser.library, RTLD_LAZY | RTLD_LOCAL);
	if (!new_lib.module) {
		LERROR("cannot open new library %s: %s", config->parser.library, dlerror());
		return 0;
	}

	if (!(new_lib.start = dlsym_m(new_lib.module, "start")) ||
			!(new_lib.configure = dlsym_m(new_lib.module, "configure")) ||
			!(new_lib.stop = dlsym_m(new_lib.module, "stop")) ||
			!(new_lib.reconfigure = dlsym_m(new_lib.module, "reconfigure"))) {
		dlclose(new_lib.module);
		return 0;
	}

	LINFO("applying new component library configuration");
	(*new_lib.configure)(config);

	if (library.module) {
		LINFO("stopping current component library");
		(*library.stop)();
	}

	LINFO("starting new component library");
	if ((*new_lib.start)()) {
		if (library.module) {
			LDEBUG("unloading old component library");
			dlclose(library.module);
		}
		library = new_lib;
		return 1;
	} else {
		LERROR("new library failed to start");
		dlclose(new_lib.module);
		if (library.module) {
			LWARN("falling back to old one");
			if (!(*library.start)()) {
				LFATAL("old library failed to start, no parsers are available. Exiting");
			}
		}
		return 0;
	}
}

static void apply_config() {
	log_level = config->logger.level;
	config->reader_thread.queue.fixed_block_buffer_size = config->reader.block;
	config->reader_thread.queue.network_buffer_size = config->reader.buffer;
	config->reader_thread.recovery_stanza_size = config->reader.max_stanza_size;
	config->reader_thread.socket = &config->writer_thread.socket;
	config->reader_thread.recovery_mode = config->reader.recovery_mode;
}

static void reload_config(int signal) {
	apply_config();

	if ((config->last_change_type & UCCA_RESTART_READER) == UCCA_RESTART_READER) {
		LWARN("some configuration changes will only be applied on wrapper restart with Recovery Mode enabled");
	}
	if ((config->last_change_type & UCCA_RELOAD_LIBRARY) == UCCA_RELOAD_LIBRARY) {
		push_library();
	} else if ((config->last_change_type & UCCA_NOTIFY_LIBRARY) == UCCA_NOTIFY_LIBRARY) {
		LINFO("reconfiguring library");
		(*library.reconfigure)();
	}
}

int wrapper_main(XmcompConfig *_config) {
	pthread_t reader;

	sighelper_sigaction(SIGHUP, reload_config);
	memset(&library, 0, sizeof(library));

	config = _config;

	apply_config();
	LINFO("wrapper started in %s mode", config->reader.recovery_mode ? "recovery" : "normal");

	queue_init(&config->reader_thread.queue, config->reader.queue);
	config->reader_thread.enabled = 1;

	if (!push_library()) {
		LFATAL("cannot start wrapper without a component library");
	}

	LINFO("starting reader thread");
	pthread_create(&reader, 0, reader_thread_entry, (void *)&config->reader_thread);

    pthread_join(reader, 0);

	LINFO("reader exited, stopping component library");
	(*library.stop)();
	dlclose(library.module);

	LINFO("sending zero byte to notify writer");
	cbuffer_write(&config->writer_thread.cbuffer, "\0", 1);

	return 0;
}
