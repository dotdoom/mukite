#include "xmcomp/src/logger.h"

#include "ut2s.h"

#include "registered_nicks.h"

BOOL registered_nicks_serialize(RegisteredNicksList *registered_nicks, FILE *output) {
	DLS_SERIALIZE(registered_nicks, RegisteredNick, registered_nick_serialize);
	return TRUE;
}

BOOL registered_nicks_deserialize(RegisteredNicksList *list, FILE *input) {
	LDEBUG("deserializing registered nicks");
	RegisteredNick *current = 0;
	DLS_DESERIALIZE(list, current, registered_nick_deserialize(current, input));
	return TRUE;
}
