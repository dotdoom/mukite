#include <stdlib.h>

#include "xmcomp/src/logger.h"

#include "serializer.h"
#include "rooms.h"
#include "builder.h"
#include "worker.h"

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
	memset(rooms, 0, sizeof(*rooms));
	pthread_rwlock_init(&rooms->sync, 0);
}

Room *rooms_create_room(Rooms *rooms, BufferPtr *node) {
	LINFO("creating new room '%.*s'", BPT_SIZE(node), node->data);

	pthread_rwlock_wrlock(&rooms->sync);
	Room *room = malloc(sizeof(*room));
	room_init(room, node);
	HASHS_ADD(rooms, node.data, BPT_SIZE(node), room);
	pthread_mutex_lock(&room->sync);
	pthread_rwlock_unlock(&rooms->sync);

	return room;
}

static void rooms_destroy_room(Rooms *rooms, Room *room) {
	pthread_rwlock_wrlock(&rooms->sync);
	HASHS_DEL(rooms, room);
	pthread_rwlock_unlock(&rooms->sync);

	room_destroy(room);
}

void rooms_destroy(Rooms *rooms) {
	Room *room = 0, *tmp = 0;
	HASHS_ITER(rooms, room, tmp) {
		rooms_destroy_room(rooms, room);
	}
	pthread_rwlock_destroy(&rooms->sync);
}

BOOL rooms_serialize(Rooms *rooms, FILE *output) {
	LDEBUG("serializing room list");
	//pthread_rwlock_rdlock(&rooms->sync);

	if (!registered_nicks_serialize(&rooms->registered_nicks, output)) {
		return FALSE;
	}

	HASHS_SERIALIZE(rooms, Room, room_serialize);
	return TRUE;
}

BOOL rooms_deserialize(Rooms *rooms, FILE *input) {
	if (!registered_nicks_deserialize(&rooms->registered_nicks, input)) {
		return FALSE;
	}

	Room *current = 0;
	HASHS_DESERIALIZE(rooms, current, node.data, current->node.size, room_deserialize(current, input));
	return TRUE;
}

void rooms_process(Rooms *rooms, IncomingPacket *ingress, ACLConfig *acl) {
	LDEBUG("searching for the existing room '%.*s'",
			JID_LEN(&ingress->proxy_to), JID_STR(&ingress->proxy_to));

	pthread_rwlock_rdlock(&rooms->sync);
	Room *room = 0;
	HASHS_FIND(rooms, ingress->proxy_to.node.data, BPT_SIZE(&ingress->proxy_to.node), room);
	pthread_rwlock_unlock(&rooms->sync);

	if (!room) {
		if (ingress->type == STANZA_ERROR) {
			return;
		}
		if (ingress->name != STANZA_PRESENCE || ingress->type == STANZA_PRESENCE_UNAVAILABLE) {
			// this is not a presence, or presence type is 'unavailable'
			worker_bounce(ingress, &error_definitions[ERROR_ROOM_NOT_FOUND], 0);
			return;
		} else {
			if (acl_role(acl, &ingress->real_from) >= ACL_MUC_CREATE) {
				room = rooms_create_room(rooms, &ingress->proxy_to.node);
			} else {
				// TODO(artem): optimization
				Buffer node;
				buffer__ptr_cpy(&node, &ingress->proxy_to.node);
				worker_bounce(ingress, &error_definitions[ERROR_ROOM_CREATE_PERMISSION], &node);
				free(node.data);
				return;
			}
		}
	}

	room_route(room, ingress, acl, rooms->limits.min_message_interval);
	pthread_mutex_unlock(&room->sync);

	if (room->flags & MUC_FLAG_DESTROYED) {
		rooms_destroy_room(rooms, room);
	}
}
