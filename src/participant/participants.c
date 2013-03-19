#include <string.h>

#include "xmcomp/src/logger.h"

#include "ut2s.h"

#include "participants.h"

Participant *participants_find_by_nick(ParticipantsList *list, BufferPtr *nick) {
	int size = BPT_SIZE(nick);
	Participant *current = 0;
	DLS_FOREACH(list, current) {
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
	DLS_FOREACH(list, current) {
		if (!jid_cmp(&current->jid, jid, mode)) {
			return current;
		}
	}

	return 0;
}

BOOL participants_serialize(ParticipantsList *participants, FILE *output) {
	DLS_SERIALIZE(participants, Participant, participant_serialize);
	return TRUE;
}

BOOL participants_deserialize(ParticipantsList *participants, FILE *input) {
	DLS_DESERIALIZE(participants, Participant, participant_deserialize);
	return TRUE;
}
