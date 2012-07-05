#ifndef NETWORK_H
#define NETWORK_H

#include "common.h"

int net_connect(char *host, int port);
int net_send(int sock, char *data, int size);
int net_recv(int sock, char *buffer, int size);
void net_disconnect(int sock);

int net_stream(int sock, char *from, char *to, char *pass);
int net_unstream(int sock);

#endif
