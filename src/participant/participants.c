#include <string.h>

#include "xmcomp/src/logger.h"

#include "dls_list.h"

#include "participants.h"

Participant *participants_find_by_nick(ParticipantsList *list, BufferPtr *nick) {
	int size = BPT_SIZE(nick);
	Participant *current = 0;
	DL_FOREACH(list->head, current) {
		if (BPT_SIZE(&current->nick) == size &&
				memcmp(current->nick.data, nick->data, size) == 0) {
			return current;
		}
	}

	return 0;
}

Participant *participants_find_by_jid(ParticipantsList *list, Jid *jid) {
	int mode = JID_FULL;

	if (BPT_NULL(&jid->resource)) {
		// it is possible that <iq> (e.g. vCard) arrives with no resource.
		// We should still be able to find that participant.
		mode = JID_NODE | JID_HOST;
		// FIXME(artem): when there are multiple joins from a single JID, this may fail
		// we should probably create a cache of IDs for the queries
	}    

	Participant *current = 0; 
	DL_FOREACH(list->head, current) {
		if (!jid_cmp(&current->jid, jid, mode)) {
			return current;
		}
	}

	return 0;
}

BOOL participants_serialize(ParticipantsList *participants, FILE *output) {
	LDEBUG("serializing participant list");
	Participant *current = 0;
	DLS_SERIALIZE(participants, current, participant_serialize(current, output));
	return TRUE;
}

BOOL participants_deserialize(ParticipantsList *participants, FILE *input) {
	LDEBUG("deserializing participant list");
	Participant *current = 0;
	DLS_DESERIALIZE(participants, current, participant_deserialize(current, input));
	return TRUE;
}