#ifndef REGISTERED_NICK_H
#define REGISTERED_NICK_H

#include "jid.h"

typedef struct RegisteredNick {
	Jid jid;
	Buffer nick;
	struct RegisteredNick *next;
} RegisteredNick;

typedef struct RegisteredNicksList {
	int size, max_size;
	RegisteredNick *head;
} RegisteredNicksList;

#endif
