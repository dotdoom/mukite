#include "registered_nick.h"

BOOL registered_nick_serialize(RegisteredNick *registered_nick, FILE *output) {
	return
		jid_serialize(&registered_nick->jid, output) &&
		buffer_serialize(&registered_nick->nick, output);
}

BOOL registered_nick_deserialize(RegisteredNick *registered_nick, FILE *input) {
	return
		jid_deserialize(&registered_nick->jid, input) &&
		buffer_deserialize(&registered_nick->nick, input, MAX_JID_PART_SIZE);
}
