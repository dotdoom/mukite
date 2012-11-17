#ifndef XMCOMP_SIGHELPER_H
#define XMCOMP_SIGHELPER_H

#include <signal.h>

typedef void (*SigHandler)(int);

// Block all signals; if parameter is not 0, do not block that signal
void sighelper_sigblockall(int except_signal);
void sighelper_sigaction(int, SigHandler);

#endif
