#include <stdlib.h>

#include "xmcomp/logger.h"
#include "router.h"
#include "serializer.h"

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
		.type = "auth",
		.text = "Room creation is denied by the service policy"
	}
};

void rooms_init(Rooms *rooms) {
	rooms->first = rooms->last = 0;
	rooms->size = 0;
	pthread_rwlock_init(&rooms->sync, 0);
}

Room *rooms_create_room(Rooms *rooms, Jid *jid) {
	Room *room = 0;

	pthread_rwlock_wrlock(&rooms->sync);
	LINFO("creating new room '%.*s'", JID_LEN(jid), JID_STR(jid));
	room = malloc(sizeof(*room));
	room_init(room, &jid->node);
	if (rooms->last) {
		rooms->last->next = room;
		room->prev = rooms->last;
	} else {
		rooms->first = room;
	}
	rooms->last = room;
	++rooms->size;
	pthread_mutex_lock(&room->sync);
	pthread_rwlock_unlock(&rooms->sync);

	return room;
}

static void rooms_destroy_room(Rooms *rooms, Room *room) {
	pthread_rwlock_wrlock(&rooms->sync);
	if (room->prev) {
		room->prev->next = room->next;
	} else {
		rooms->first = room->next;
	}
	if (room->next) {
		room->next->prev = room->prev;
	} else {
		rooms->last = room->prev;
	}
	--rooms->size;
	room_destroy(room);
	pthread_rwlock_unlock(&rooms->sync);
}

void rooms_destroy(Rooms *rooms) {
	pthread_rwlock_destroy(&rooms->sync);
	while (rooms->first) {
		rooms_destroy_room(rooms, rooms->first);
	}
}

BOOL registered_nicks_serialize(RegisteredNick *list, FILE *output) {
	LDEBUG("serializing registered nicks");
	SERIALIZE_LIST(
		jid_serialize(&list->jid, output) &&
		buffer_serialize(&list->nick, output)
	);
	return TRUE;
}

BOOL registered_nicks_deserialize(RegisteredNick **list, FILE *input, int limit) {
	int entry_count = 0;
	RegisteredNick *new_entry = 0;
	LDEBUG("deserializing registered nicks");
	DESERIALIZE_LIST(
		jid_deserialize(&new_entry->jid, input) &&
		buffer_deserialize(&new_entry->nick, input, MAX_JID_PART_SIZE),
	);
	return TRUE;
}

BOOL rooms_serialize(Rooms *rooms, FILE *output) {
	Room *list = rooms->first;
	LDEBUG("serializing room list");
	if (!registered_nicks_serialize(rooms->registered_nicks, output)) {
		return FALSE;
	}
	SERIALIZE_LIST(
		room_serialize(list, output)
	);
	return TRUE;
}

BOOL rooms_deserialize(Rooms *rooms, FILE *input, int limit) {
	int entry_count = 0;
	Room *new_entry = 0;
	Room **list = &rooms->first;
	LDEBUG("deserializing room list");
	if (!registered_nicks_deserialize(&rooms->registered_nicks, input, MAX_REGISTERED_NICKS)) {
		return FALSE;
	}
	DESERIALIZE_LIST(
		room_deserialize(new_entry, input),
	);
	rooms->last = new_entry;
	rooms->size = entry_count;
	return TRUE;
}

void rooms_route(RouterChunk *chunk) {
	Room *room = 0;
	Rooms *rooms = &chunk->config->rooms;
	IncomingPacket *ingress = &chunk->ingress;
	BuilderPacket *egress = &chunk->egress;

	pthread_rwlock_rdlock(&rooms->sync);
	LDEBUG("searching for the existing room '%.*s'",
			JID_LEN(&ingress->proxy_to), JID_STR(&ingress->proxy_to));
	for (room = rooms->first; room; room = room->next) {
		if (!jid_strcmp(&ingress->proxy_to, &room->node, JID_NODE)) {
			pthread_mutex_lock(&room->sync);
			if (room->flags & MUC_FLAG_DESTROYED) {
				// This isn't the room you're looking for
				pthread_mutex_unlock(&room->sync);
			} else {
				break;
			}
		}
	}
	pthread_rwlock_unlock(&rooms->sync);

	if (!room) {
		if (ingress->name != 'p' || ingress->type == 'u') {
			// this is not a presence, or presence type is 'unavailable'
			router_error(chunk, &error_definitions[ERROR_ROOM_NOT_FOUND]);
			return;
		} else {
			if (acl_role(chunk->acl, &ingress->real_from) >= ACL_MUC_CREATE) {
				room = rooms_create_room(rooms, &ingress->proxy_to);
			} else {
				buffer__ptr_cpy(&egress->from_node, &ingress->proxy_to.node);
				buffer_ptr_cpy(&egress->from_nick, &ingress->proxy_to.resource);
				router_error(chunk, &error_definitions[ERROR_ROOM_CREATE_PERMISSION]);
				free(egress->from_node.data);
				free(egress->from_nick.data);
				return;
			}
		}
	}

	egress->from_node = room->node;
	room_route(room, chunk);
	pthread_mutex_unlock(&room->sync);

	if (room->flags & MUC_FLAG_DESTROYED) {
		rooms_destroy_room(rooms, room);
	}
}
