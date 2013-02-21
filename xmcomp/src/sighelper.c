#include <string.h>

#include "sighelper.h"

void sighelper_sigblockall(int except_signal) {
	sigset_t block_sigset;

	sigfillset(&block_sigset);
	if (except_signal) {
		sigdelset(&block_sigset, except_signal);
	}
	pthread_sigmask(SIG_SETMASK, &block_sigset, 0);
}

void sighelper_sigaction(int signal, SigHandler handler) {
	struct sigaction action = {};
	action.sa_handler = handler;
	// Safety: do not allow other signals while current handler is active
	sigfillset(&action.sa_mask);
	sigaction(signal, &action, 0);
}
