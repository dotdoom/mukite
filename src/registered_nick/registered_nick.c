#include "uthash/src/uthash.h"

#include "xmcomp/src/logger.h"

#include "serializer.h"
#include "rooms.h"
#include "builder.h"
#include "worker.h"

BOOL registered_nicks_serialize(RegisteredNicksList *list, FILE *output) {
	LDEBUG("serializing registered nicks");
	RegisteredNick *current = 0;
	LL_SERIALIZE(list, current,
		jid_serialize(&current->jid, output) &&
		buffer_serialize(&current->nick, output)
	);
	return TRUE;
}

BOOL registered_nicks_deserialize(RegisteredNicksList *list, FILE *input) {
	LDEBUG("deserializing registered nicks");
	RegisteredNick *current = 0;
	LL_DESERIALIZE(list, current,
		jid_deserialize(&current->jid, input) &&
		buffer_deserialize(&current->nick, input, MAX_JID_PART_SIZE),
	);
	return TRUE;
}
