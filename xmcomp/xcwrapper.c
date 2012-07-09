#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#include "reader.h"
#include "logger.h"
#include "sighelper.h"

#include "xcwrapper.h"

typedef void* (*InitializeFunction)(XmcompConfig *);
typedef int (*ActionFunction)(void *);

struct LibraryEntry {
	void *module;
	InitializeFunction initialize;
	ActionFunction start, stop, reconfigure, finalize;
	void *data;
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

static BOOL switch_library(char *pathname) {
	LINFO("(re)loading component library");

	struct LibraryEntry new_lib;

	if (pathname) {
		new_lib.module = dlopen(pathname, RTLD_LAZY | RTLD_LOCAL);
		if (!new_lib.module) {
			LERROR("cannot open new library (%s): %s", pathname, dlerror());
			return FALSE;
		}

		if (new_lib.module == library.module) {
			LERROR("new library is the same file, aborting reload");
			return FALSE;
		}

		if (!(new_lib.initialize = dlsym_m(new_lib.module, "initialize")) ||
				!(new_lib.start = dlsym_m(new_lib.module, "start")) ||
				!(new_lib.reconfigure = dlsym_m(new_lib.module, "reconfigure")) ||
				!(new_lib.stop = dlsym_m(new_lib.module, "stop")) ||
				!(new_lib.finalize = dlsym_m(new_lib.module, "finalize"))) {
			dlclose(new_lib.module);
			return FALSE;
		}

		LINFO("new library: initialize");
		new_lib.data = (*new_lib.initialize)(config);
	} else {
		new_lib.module = 0;
	}

	if (library.module) {
		LINFO("current library: stop");
		(*library.stop)(library.data);
	}

	LINFO("new library: start");
	if (!new_lib.module || (*new_lib.start)(new_lib.data)) {
		if (library.module) {
			LDEBUG("old library: finalize");
			(*library.finalize)(library.data);
			LDEBUG("old library: unload");
			dlclose(library.module);
		}
		library = new_lib;
		return TRUE;
	} else {
		LERROR("new library failed to start, finalizing and exiting");
		(*new_lib.finalize)(new_lib.data);
		dlclose(new_lib.module);
		if (library.module) {
			LWARN("falling back to old library");
			if (!(*library.start)(library.data)) {
				LFATAL("old library failed to restart, no parsers are available now. Exiting");
			}
		}
		return FALSE;
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
		switch_library(config->parser.library);
	} else if ((config->last_change_type & UCCA_NOTIFY_LIBRARY) == UCCA_NOTIFY_LIBRARY) {
		LINFO("reconfiguring library");
		(*library.reconfigure)(library.data);
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

	if (!switch_library(config->parser.library)) {
		LFATAL("cannot start wrapper without a component library");
	}

	LINFO("starting reader thread");
	pthread_create(&reader, 0, reader_thread_entry, (void *)&config->reader_thread);

    pthread_join(reader, 0);

	LINFO("reader exited, stopping component library");
	switch_library(0);

	LINFO("sending zero byte to notify writer");
	cbuffer_write(&config->writer_thread.cbuffer, "\0", 1);

	return 0;
}
