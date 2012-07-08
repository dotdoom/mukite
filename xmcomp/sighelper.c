#include <string.h>

#include "sighelper.h"

void sighelper_sigblockall() {
	sigset_t block_sigset;

	sigfillset(&block_sigset);
	pthread_sigmask(SIG_SETMASK, &block_sigset, 0);
}

void sighelper_sigaction(int signal, SigHandler handler) {
	struct sigaction action;
	memset(&action, 0, sizeof(action));
	action.sa_handler = handler;
	sigemptyset(&action.sa_mask);
	// Safety: do not allow signal handler to be reentrant
	sigaddset(&action.sa_mask, signal);
	sigaction(signal, &action, 0);
}
