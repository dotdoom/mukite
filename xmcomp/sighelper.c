#include <string.h>

#include "sighelper.h"

void sighelper_sigblock(int signal) {
	sigset_t block_sigset;

	sigemptyset(&block_sigset);
	sigaddset(&block_sigset, signal);
	pthread_sigmask(SIG_BLOCK, &block_sigset, 0);
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
