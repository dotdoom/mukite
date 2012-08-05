#include <stdlib.h>
#include <string.h>

#include "xmcomp/logger.h"

#include "room.h"

void room_init(Room *room, BufferPtr *node) {
	memset(room, 0, sizeof(*room));
	pthread_mutex_init(&room->sync, 0);
	room->node.size = BPT_SIZE(node);
	room->node.data = malloc(room->node.size);
	memcpy(room->node.data, node->data, room->node.size);
//	room->flags = MUC_FLAG_SEMIANONYMOUS;

	room->default_role = ROLE_PARTICIPANT;
}

inline AffiliationEntry *find_affiliation(AffiliationEntry *entry, Jid *jid) {
	for (; entry; entry = entry->next) {
		if (jid_cmp(&entry->jid, jid, JID_FULL | JID_CMP_NULLWC)) {
			return entry;
		}
	}
	return 0;
}

int room_get_affiliation(Room *room, Jid *jid) {
	AffiliationEntry *entry = 0;

	if ((entry = find_affiliation(room->members, jid))) {
		return AFFIL_MEMBER;
	}

	if ((entry = find_affiliation(room->outcasts, jid))) {
		return AFFIL_OUTCAST;
	}

	if ((entry = find_affiliation(room->admins, jid))) {
		return AFFIL_ADMIN;
	}

	if ((entry = find_affiliation(room->owners, jid))) {
		return AFFIL_OWNER;
	}

	return AFFIL_NONE;
}

ParticipantEntry *room_join(Room *room, Jid *jid, BufferPtr *nick) {
	ParticipantEntry *participant = malloc(sizeof(*participant));

	LDEBUG("'%.*s' is joining the room as '%.*s'",
			JID_LEN(jid), JID_STR(jid),
			BPT_SIZE(nick), nick->data);

	jid_cpy(&participant->jid, jid);

	participant->nick.size = BPT_SIZE(nick);
	participant->nick.data = malloc(participant->nick.size);
	memcpy(participant->nick.data, nick->data, participant->nick.size);

	if (room->participants) {
		room->participants->prev = participant;
	}
	participant->next = room->participants;
	participant->prev = 0;
	room->participants = participant;

	participant->role = room->default_role;
	participant->affiliation = room_get_affiliation(room, jid);

	LDEBUG("created new participant, role %d affil %d", participant->role, participant->affiliation);

	return participant;
}

void room_leave(Room *room, ParticipantEntry *participant) {
	if (room->participants == participant) {
		room->participants = participant->next;
	} else {
		participant->prev->next = participant->next;
	}
	if (participant->next) {
		participant->next->prev = participant->prev;
	}

	LDEBUG("'%.*s' (rJID '%.*s') has left '%.*s'",
			participant->nick.size, participant->nick.data,
			JID_LEN(&participant->jid), JID_STR(&participant->jid),
			room->node.size, room->node.data);

	jid_free(&participant->jid);
	free(participant->nick.data);
}

ParticipantEntry *room_participant_by_nick(Room *room, BufferPtr *nick) {
	ParticipantEntry *current = room->participants;
	for (; current; current = current->next) {
		if (current->nick.size == BPT_SIZE(nick)) {
			if (!memcmp(current->nick.data, nick->data, current->nick.size)) {
				return current;
			}
		}
	}

	return 0;
}

ParticipantEntry *room_participant_by_jid(Room *room, Jid *jid) {
	ParticipantEntry *current = room->participants;

	LDEBUG("finding rJID '%.*s' in room '%.*s'",
			JID_LEN(jid), JID_STR(jid),
			room->node.size, room->node.data);
	for (; current; current = current->next) {
		if (!jid_cmp(&current->jid, jid, JID_FULL)) {
			return current;
		}
	}

	return 0;
}
