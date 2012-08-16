#include <stdlib.h>

#include "xmcomp/logger.h"
#include "router.h"

#include "rooms.h"

static XMPPError error_definitions[] = {
	{
#define ERROR_ROOM_NOT_FOUND 0
		.code = "405",
		.name = "item-not-found",
		.type = "cancel",
		.text = "Room does not exist"
	}, {
#define ERROR_ROOM_CREATE_PERMISSION 1
		.code = "403",
		.name = "forbidden",
		.type = "cancel",
		.text = "Room creation is denied by service policy"
	}
};

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

void rooms_route(RouterChunk *chunk) {
	Room *room = 0;
	Rooms *rooms = chunk->rooms;
	IncomingPacket *input = &chunk->input;

	if (!(room = rooms_find(rooms, &input->proxy_to))) {
		if (input->name != 'p' || input->type == 'u') {
			// this is not a presence, or presence type is 'unavailable'
			router_error(chunk, &error_definitions[ERROR_ROOM_NOT_FOUND]);
			return;
		} else {
			if ((acl_role(chunk->acl, &input->real_from) & ACL_MUC_CREATE) == ACL_MUC_CREATE) {
				room = rooms_create(rooms, &input->proxy_to);
			} else {
				router_error(chunk, &error_definitions[ERROR_ROOM_CREATE_PERMISSION]);
				return;
			}
		}
	}

	rooms_acquire(room);
	room_route(room, chunk);
	// TODO(artem); if (!room->participants && !(room->flags & MUC_FLAG_PERSISTENTROOM)) then remove the room
	rooms_release(room);
}
