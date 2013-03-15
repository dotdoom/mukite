#ifndef REGISTERED_NICKS_H
#define REGISTERED_NICKS_H

#include "registered_nick.h"

#define MAX_REGISTERED_NICKS 10240

typedef struct RegisteredNicksList {
	DLS_DECLARE(RegisteredNick);
} RegisteredNicksList;

BOOL registered_nicks_serialize(RegisteredNicksList *, FILE *);
BOOL registered_nicks_deserialize(RegisteredNicksList *, FILE *);

#endif
