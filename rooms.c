#include <stdlib.h>

#include "xmcomp/logger.h"

#include "rooms.h"

void rooms_init(Rooms *rooms) {
	rooms->start = rooms->end = 0;
	rooms->count = 0;
	pthread_mutex_init(&rooms->sync, 0);
}

Room *rooms_acquire(Rooms *rooms, Jid *jid) {
	Room *room = 0;

	pthread_mutex_lock(&rooms->sync);

	LDEBUG("searching for the room");
	for (room = rooms->start; room; room = room->next) {
		if (!jid_strcmp(jid, &room->node, JID_NODE)) {
			break;
		}
	}

	if (!room) {
		LINFO("creating new room '%.*s'", JID_LEN(jid), JID_STR(jid));
		room = malloc(sizeof(*room));
		room_init(room, &jid->node);

		if (rooms->end) {
			rooms->end->next = room;
			room->prev = rooms->end;
		} else {
			rooms->start = room;
		}
		rooms->end = room;
	}

	pthread_mutex_unlock(&rooms->sync);

	pthread_mutex_lock(&room->sync);
	return room;
}

void rooms_release(Room *room) {
	pthread_mutex_unlock(&room->sync);
}

BOOL rooms_serialize(Rooms *rooms, FILE *output) {
	return TRUE;
}

BOOL rooms_deserialize(Rooms *rooms, FILE *input, int limit) {
	return TRUE;
}
