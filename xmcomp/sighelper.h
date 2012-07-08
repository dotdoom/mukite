#ifndef XMCOMP_SIGHELPER_H
#define XMCOMP_SIGHELPER_H

#include <signal.h>

typedef void (*SigHandler)(int);

void sighelper_sigblock(int);
void sighelper_sigaction(int, SigHandler);

#endif
