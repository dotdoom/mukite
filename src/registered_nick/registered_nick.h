#ifndef REGISTERED_NICK_H
#define REGISTERED_NICK_H

#include "jid.h"

typedef struct RegisteredNick {
	Jid jid;
	Buffer nick;
	struct RegisteredNick *prev, *next;
} RegisteredNick;

BOOL registered_nick_serialize(RegisteredNick *, FILE *);
BOOL registered_nick_deserialize(RegisteredNick *, FILE *);

#endif
