#include <stdlib.h>
#include <string.h>

#include "xmcomp/src/logger.h"

#include "serializer.h"

#include "participant.h"

Participant *participant_init(Participant *participant, Jid *jid) {
	memset(participant, 0, sizeof(*participant));
	jid_set(&participant->jid, jid, JID_FULL);
	return participant;
}

Participant *participant_destroy(Participant *participant) {
	free(participant->nick.data);
	free(participant->presence.data);
	return participant;
}

BOOL participant_deserialize(Participant *participant, FILE *input) {
	return
		jid_deserialize(&participant->jid, input) &&
		buffer_ptr_deserialize(&participant->nick, input, MAX_JID_PART_SIZE) &&
		buffer_ptr_deserialize(&participant->presence, input, MAX_PRESENCE_SIZE) &&
        DESERIALIZE_BASE(participant->affiliation) &&
        DESERIALIZE_BASE(participant->role);
}

BOOL participant_serialize(Participant *participant, FILE *output) {
	return
		jid_serialize(&participant->jid, output) &&
		buffer_ptr_serialize(&participant->nick, output) &&
		buffer_ptr_serialize(&participant->presence, output) &&
		SERIALIZE_BASE(participant->affiliation) &&
		SERIALIZE_BASE(participant->role);
}

BOOL participant_set_nick(Participant *participant, BufferPtr *nick) {
	BPT_SETTER(participant->nick, nick, MAX_JID_PART_SIZE);
}

BOOL participant_set_presence(Participant *participant, BufferPtr *presence) {
	BPT_SETTER(participant->presence, presence, MAX_PRESENCE_SIZE);
}

void participant_set_affected(Participant **first_affected, Participant **current_affected,
		Participant *new_affected, BufferPtr *reason_node) {
	if (!new_affected->affected_list.included) {
		new_affected->affected_list.included = TRUE;
		if (reason_node) {
			new_affected->affected_list.reason_node = *reason_node;
		} else {
			BPT_INIT(&new_affected->affected_list.reason_node);
		}
		new_affected->affected_list.next = 0;
		if (*first_affected) {
			(*current_affected)->affected_list.next = new_affected;
			*current_affected = new_affected;
		} else {
			*first_affected = *current_affected = new_affected;
		}
	}
}
