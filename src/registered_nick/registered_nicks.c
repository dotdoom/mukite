#include "xmcomp/src/logger.h"

#include "dls_list.h"

#include "registered_nicks.h"

BOOL registered_nicks_serialize(RegisteredNicksList *list, FILE *output) {
	LDEBUG("serializing registered nicks");
	RegisteredNick *current = 0;
	DLS_SERIALIZE(list, current, registered_nick_serialize(current, output));
	return TRUE;
}

BOOL registered_nicks_deserialize(RegisteredNicksList *list, FILE *input) {
	LDEBUG("deserializing registered nicks");
	RegisteredNick *current = 0;
	DLS_DESERIALIZE(list, current, registered_nick_deserialize(current, input));
	return TRUE;
}
