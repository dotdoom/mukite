#ifndef XMCOMP_NETWORK_H
#define XMCOMP_NETWORK_H

#include "common.h"

typedef struct {
	int socket;
	BOOL connected;
} Socket;

BOOL net_connect(Socket *, char *, int);
int net_send(Socket *, char *, int);
int net_recv(Socket *, char *, int);
void net_disconnect(Socket *);

BOOL net_stream(Socket *sock, char *from, char *to, char *password);
BOOL net_unstream(Socket *);

#endif
