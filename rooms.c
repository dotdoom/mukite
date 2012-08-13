#include <stdlib.h>

#include "xmcomp/logger.h"
#include "router.h"

#include "rooms.h"

void rooms_init(Rooms *rooms) {
	rooms->start = rooms->end = 0;
	rooms->count = 0;
	pthread_mutex_init(&rooms->sync, 0);
}

Room *rooms_find(Rooms *rooms, Jid *jid) {
	Room *room = 0;

	pthread_mutex_lock(&rooms->sync);
	LDEBUG("searching for the existing room '%.*s'", JID_LEN(jid), JID_STR(jid));
	for (room = rooms->start; room; room = room->next) {
		if (!jid_strcmp(jid, &room->node, JID_NODE)) {
			break;
		}
	}
	pthread_mutex_unlock(&rooms->sync);

	return room;
}

Room *rooms_create(Rooms *rooms, Jid *jid) {
	Room *room = 0;

	pthread_mutex_lock(&rooms->sync);
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
	pthread_mutex_unlock(&rooms->sync);

	return room;
}

void rooms_acquire(Room *room) {
	pthread_mutex_lock(&room->sync);
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

int rooms_route(RouterChunk *chunk) {
	Room *room = 0;
	Rooms *rooms = chunk->rooms;
	IncomingPacket *packet = &chunk->packet;
	int routed = 0;

	if (!(room = rooms_find(rooms, &chunk->packet.proxy_to))) {
		if (packet->name != 'p' || packet->type == 'u') {
			// this is not a presence, or presence type is 'unavailable'
			return router_error(chunk, "cancel", "item-not-found");
		} else {
			if ((acl_role(chunk->acl, &chunk->packet.real_from) & ACL_MUC_CREATE) == ACL_MUC_CREATE) {
				room = rooms_create(rooms, &chunk->packet.proxy_to);
			} else {
				return router_error(chunk, "cancel", "forbidden");
			}
		}
	}

	rooms_acquire(room);
	routed = room_route(room, chunk);
	// TODO(artem); if (!room->participants && !(room->flags & MUC_FLAG_PERSISTENTROOM)) then remove the room
	rooms_release(room);

	return routed;
}
