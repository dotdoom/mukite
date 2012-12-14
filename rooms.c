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
		.type = "cancel",
		.text = "Room creation is denied by service policy"
	}
};

/*
	<iq from="comicslate.org" type="result" to="dot@sys/Laptop" id="aad1a">
		<query xmlns="http://jabber.org/protocol/disco#info">
			<identity category="conference" type="text" name="Chatrooms"/>
			<feature var="http://jabber.org/protocol/disco#info"/>
			<feature var="http://jabber.org/protocol/disco#items"/>
			<feature var="http://jabber.org/protocol/muc"/>
			<feature var="http://jabber.org/protocol/muc#unique"/>
			<feature var="jabber:iq:register"/>
			<feature var="http://jabber.org/protocol/rsm"/>
			<feature var="vcard-temp"/>
			<x xmlns="jabber:x:data" type="result">
				<field type="hidden" var="FORM_TYPE">
					<value>http://jabber.org/network/serverinfo</value>
				</field>
			</x>
		</query>
	</iq>


	<iq from="comicslate.org" type="result" to="dot@sys/Laptop" id="aad2a">
		<query xmlns="http://jabber.org/protocol/disco#items">
			<item name="chat (8)" jid="chat@comicslate.org"/>
		</query>
	</iq>
*/

void rooms_init(Rooms *rooms) {
	rooms->start = rooms->end = 0;
	rooms->count = 0;
	pthread_rwlock_init(&rooms->sync, 0);
}

void rooms_destroy(Rooms *rooms) {
	pthread_rwlock_destroy(&rooms->sync);
	// TODO(artem): may want to clean all the rooms
}

Room *rooms_find(Rooms *rooms, Jid *jid) {
	Room *room = 0;

	pthread_rwlock_rdlock(&rooms->sync);
	LDEBUG("searching for the existing room '%.*s'", JID_LEN(jid), JID_STR(jid));
	for (room = rooms->start; room; room = room->next) {
		if (!jid_strcmp(jid, &room->node, JID_NODE)) {
			break;
		}
	}
	pthread_rwlock_unlock(&rooms->sync);

	return room;
}

Room *rooms_create(Rooms *rooms, Jid *jid) {
	Room *room = 0;

	pthread_rwlock_wrlock(&rooms->sync);
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
	pthread_rwlock_unlock(&rooms->sync);

	return room;
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
	Room *list = rooms->start;
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
	Room **list = &rooms->start;
	LDEBUG("deserializing room list");
	if (!registered_nicks_deserialize(&rooms->registered_nicks, input, MAX_REGISTERED_NICKS)) {
		return FALSE;
	}
	DESERIALIZE_LIST(
		room_deserialize(new_entry, input),
	);
	rooms->end = new_entry;
	return TRUE;
}

void rooms_acquire(Room *room) {
	pthread_mutex_lock(&room->sync);
}

void rooms_release(Room *room) {
	pthread_mutex_unlock(&room->sync);
}

void rooms_route(RouterChunk *chunk) {
	Room *room = 0;
	Rooms *rooms = &chunk->config->rooms;
	IncomingPacket *ingress = &chunk->ingress;
	BuilderPacket *egress = &chunk->egress;

	if (!(room = rooms_find(rooms, &ingress->proxy_to))) {
		if (ingress->name != 'p' || ingress->type == 'u') {
			// this is not a presence, or presence type is 'unavailable'
			router_error(chunk, &error_definitions[ERROR_ROOM_NOT_FOUND]);
			return;
		} else {
			if ((acl_role(chunk->acl, &ingress->real_from) & ACL_MUC_CREATE) == ACL_MUC_CREATE) {
				room = rooms_create(rooms, &ingress->proxy_to);
			} else {
				router_error(chunk, &error_definitions[ERROR_ROOM_CREATE_PERMISSION]);
				return;
			}
		}
	}

	rooms_acquire(room);
	egress->from_node = room->node;
	room_route(room, chunk);
	// TODO(artem); if (!room->participants && !(room->flags & MUC_FLAG_PERSISTENTROOM)) then remove the room
	rooms_release(room);
}
