#include "xmcomp/src/logger.h"

#include "utss.h"

#include "registered_nicks.h"

BOOL registered_nicks_serialize(RegisteredNicksList *registered_nicks, FILE *output) {
	DLS_SERIALIZE(registered_nicks, RegisteredNick, registered_nick_serialize);
	return TRUE;
}

BOOL registered_nicks_deserialize(RegisteredNicksList *registered_nicks, FILE *input) {
	DLS_DESERIALIZE(registered_nicks, RegisteredNick, registered_nick_deserialize);
	return TRUE;
}
