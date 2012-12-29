#include <stdlib.h>
#include <string.h>

#include "xmcomp/logger.h"
#include "router.h"
#include "serializer.h"

#include "room.h"

const char* affiliation_names[] = {
	"outcast",
	"none",
	"member",
	"admin",
	"owner"
};
const int affiliation_name_sizes[] = {
	7,
	4,
	6,
	5,
	5
};

const char* role_names[] = {
	"none",
	"visitor",
	"participant",
	"moderator"
};
const int role_name_sizes[] = {
	4,
	7,
	11,
	9
};

// http://xmpp.org/extensions/xep-0045.html#registrar-statuscodes-init
#define STATUS_NON_ANONYMOUS 100
#define STATUS_SELF_PRESENCE 110
#define STATUS_LOGGING_ENABLED 170
#define STATUS_ROOM_CREATED 201
#define STATUS_NICKNAME_ENFORCED 210
#define STATUS_BANNED 301
#define STATUS_NICKNAME_CHANGED 303
#define STATUS_KICKED 307
#define STATUS_NONMEMBER_REMOVED 321

static XMPPError error_definitions[] = {
	{
#define ERROR_EXTERNAL_MESSAGE 0
		.code = "406",
		.name = "not-acceptable",
		.type = "modify",
		.text = "Only participants are allowed to send messages to this conference"
	}, {
#define ERROR_RECIPIENT_NOT_IN_ROOM 1
		.code = "404",
		.name = "item-not-found",
		.type = "cancel",
		.text = "Recipient is not in the conference room"
	}, {
#define ERROR_NO_VISITORS_PM 2
		.code = "403",
		.name = "forbidden",
		.type = "cancel",
		.text = "Visitors are not allowed to send private messages in this room"
	}, {
#define ERROR_NO_VISITORS_PUBLIC 3
		.code = "403",
		.name = "forbidden",
		.type = "cancel",
		.text = "Visitors are not allowed to send messages to the public chat in a moderated room"
	}, {
#define ERROR_OCCUPANT_CONFLICT 4
		.code = "",
		.name = "conflict",
		.type = "modify",
		.text = "This nickname is already in use by another occupant"
	}, {
#define ERROR_OCCUPANTS_LIMIT 5
		.code = "",
		.name = "not-allowed",
		.type = "wait",
		.text = "Maximum number of occupants has been reached for this room"
	}, {
#define ERROR_BANNED 6
		.code = "403",
		.name = "forbidden",
		.type = "cancel",
		.text = "You are banned from this room"
	}, {
#define ERROR_MEMBERS_ONLY 7
		.code = "403",
		.name = "forbidden",
		.type = "cancel",
		.text = "Only members are allowed to enter this room"
	}, {
#define ERROR_EXTERNAL_IQ 8
		.code = "406",
		.name = "not-acceptable",
		.type = "modify",
		.text = "Only occupants are allowed to send queries to the conference"
	}, {
#define ERROR_IQ_PROHIBITED 9
		.code = "405",
		.name = "not-allowed",
		.type = "cancel",
		.text = "Queries to the conference occupants are not allowed in this room"
	}, {
#define ERROR_IQ_BAD 10
		.code = "400",
		.name = "bad-request",
		.type = "modify",
		.text = ""
	}, {
#define ERROR_PRIVILEGE_LEVEL 11
		.code = "403",
		.name = "forbidden",
		.type = "cancel",
		.text = "This kind of action requires higher privileges than you have"
	}, {
#define ERROR_TRAFFIC_RATE 12
		.code = "500",
		.name = "resource-constraint",
		.type = "wait",
		.text = "Traffic rate limit exceeded"
	}, {
#define ERROR_NO_VISITORS_PRESENCE 13
		.code = "403",
		.name = "forbidden",
		.type = "cancel",
		.text = "Visitors are not allowed to update their presence in this room"
	}, {
#define ERROR_PASSWORD 14
		.code = "403",
		.name = "not-authorized",
		.type = "modify",
		.text = "This room is password protected and the password was not supplied or is incorrect"
	}, {
#define ERROR_NO_PM 15
		.code = "403",
		.name = "forbidden",
		.type = "cancel",
		.text = "Private messages are not allowed in this room"
	}, {
#define ERROR_NOT_IMPLEMENTED 16
		.code = "501",
		.name = "feature-not-implemented",
		.type = "cancel",
		.text = "This service does not provide the requested feature"
	}, {
#define ERROR_JID_MALFORMED 17
		.code = "400",
		.name = "jid-malformed",
		.type = "modify",
		.text = "You should specify a room nickname"
	}
};

#define SEND(send) (send)->proc((send)->data)

void room_init(Room *room, BufferPtr *node) {
	memset(room, 0, sizeof(*room));
	pthread_mutex_init(&room->sync, 0);
	room->node.size = BPT_SIZE(node);
	room->node.data = malloc(room->node.size);
	memcpy(room->node.data, node->data, room->node.size);
	room->flags = MUC_FLAGS_DEFAULT;
	room->participants.max_size = 100;
	room->history.max_size = 20;
	room->default_role = ROLE_PARTICIPANT;
}

void history_shift(struct HistoryList *history) {
	HistoryEntry *removee = 0;
	if (history->first) {
		removee = history->first;
		if (removee->next) {
			removee->next->prev = 0;
		} else {
			history->last = 0;
		}
		history->first = removee->next;

		free(removee->nick.data);
		free(removee->header.data);
		free(removee->inner.data);
		free(removee);
		--history->size;
	}
}

static AffiliationEntry *room_affiliation_detach_clear(AffiliationEntry **list,
		AffiliationEntry *affiliation, AffiliationEntry *prev) {
	if (prev) {
		prev->next = affiliation->next;
	} else {
		*list = affiliation->next;
	}
	jid_destroy(&affiliation->jid);
	free(affiliation->reason.data);
	return affiliation;
}

static void room_participants_clear(Room *room) {
	while (room->participants.first) {
		room_leave(room, room->participants.first);
	}
}

void room_destroy(Room *room) {
	int i;

	LDEBUG("destroying room '%.*s'", room->node.size, room->node.data);
	room_participants_clear(room);
	free(room->node.data);
	free(room->title.data);
	free(room->description.data);
	free(room->subject.data.data);
	free(room->subject.nick.data);
	free(room->password.data);
	for (i = AFFIL_OUTCAST; i <= AFFIL_OWNER; ++i) {
		while (room->affiliations[i]) {
			free(room_affiliation_detach_clear(&room->affiliations[i], room->affiliations[i], 0));
		}
	}
	while (room->history.size) {
		history_shift(&room->history);
	}
	pthread_mutex_destroy(&room->sync);
}

int room_role_for_affiliation(Room *room, int affiliation) {
	switch (affiliation) {
		case AFFIL_MEMBER:
			return ROLE_PARTICIPANT;
		case AFFIL_ADMIN:
		case AFFIL_OWNER:
			return ROLE_MODERATOR;
	}
	return room->default_role;
}

inline static AffiliationEntry *find_affiliation(AffiliationEntry *entry, Jid *jid, int mode) {
	for (; entry; entry = entry->next) {
		if (!jid_cmp(&entry->jid, jid, mode)) {
			return entry;
		}
	}
	return 0;
}

static int room_get_affiliation(Room *room, ACLConfig *acl, Jid *jid) {
	AffiliationEntry *entry = 0;
	int affiliation;

	if (acl_role(acl, jid) >= ACL_MUC_ADMIN) {
		return AFFIL_OWNER;
	}

	for (affiliation = AFFIL_OUTCAST; affiliation <= AFFIL_OWNER; ++affiliation) {
		if ((entry = find_affiliation(room->affiliations[affiliation], jid,
						JID_NODE | JID_HOST))) {
			return affiliation;
		}
	}

	for (affiliation = AFFIL_OUTCAST; affiliation <= AFFIL_OWNER; ++affiliation) {
		if ((entry = find_affiliation(room->affiliations[affiliation], jid,
						JID_NODE | JID_HOST | JID_CMP_NULLWC))) {
			return affiliation;
		}
	}

	return AFFIL_NONE;
}


ParticipantEntry *room_join(Room *room, Jid *jid, BufferPtr *nick, int affiliation) {
	ParticipantEntry *participant = malloc(sizeof(*participant));
	memset(participant, 0, sizeof(*participant));

	LDEBUG("'%.*s' is joining the room '%.*s' as '%.*s'",
			JID_LEN(jid), JID_STR(jid),
			room->node.size, room->node.data,
			BPT_SIZE(nick), nick->data);

	jid_cpy(&participant->jid, jid, JID_FULL);

	buffer_ptr_cpy(&participant->nick, nick);

	if (room->participants.first) {
		room->participants.first->prev = participant;
	}
	participant->next = room->participants.first;
	room->participants.first = participant;

	++room->participants.size;

	participant->affiliation = affiliation;
	participant->role = room_role_for_affiliation(room, affiliation);

	LDEBUG("created new participant #%d, role %d affil %d",
			room->participants.size, participant->role, participant->affiliation);

	return participant;
}

void room_leave(Room *room, ParticipantEntry *participant) {
	if (participant->prev) {
		participant->prev->next = participant->next;
	} else {
		room->participants.first = participant->next;
	}
	if (participant->next) {
		participant->next->prev = participant->prev;
	}

	LDEBUG("participant #%d '%.*s' (JID '%.*s') has left '%.*s'",
			room->participants.size, BPT_SIZE(&participant->nick), participant->nick.data,
			JID_LEN(&participant->jid), JID_STR(&participant->jid),
			room->node.size, room->node.data);
	--room->participants.size;

	jid_destroy(&participant->jid);
	free(participant->presence.data);
	free(participant->nick.data);

	if (!(room->flags & MUC_FLAG_PERSISTENTROOM) &&
			!room->participants.size) {
		room->flags |= MUC_FLAG_DESTROYED;
	}
}

ParticipantEntry *room_participant_by_nick(Room *room, BufferPtr *nick) {
	int size = BPT_SIZE(nick);
	ParticipantEntry *current = room->participants.first;
	for (; current; current = current->next) {
		if (BPT_SIZE(&current->nick) == size &&
				!memcmp(current->nick.data, nick->data, size)) {
			return current;
		}
	}

	return 0;
}

ParticipantEntry *room_participant_by_jid(Room *room, Jid *jid) {
	ParticipantEntry *current = room->participants.first;
	int mode = JID_FULL;

	if (BPT_NULL(&jid->resource)) {
		// it is possible that <iq> (e.g. vCard) come with no resource.
		// We should still be able to find that participant.
		mode = JID_NODE | JID_HOST;
		// FIXME(artem): when there are multiple joins from a single JID, this may fail
		// we should probably create a cache of IDs for the queries
	}

	LDEBUG("finding rJID '%.*s' in room '%.*s', mode %d",
			JID_LEN(jid), JID_STR(jid),
			room->node.size, room->node.data, mode);
	for (; current; current = current->next) {
		if (!jid_cmp(&current->jid, jid, mode)) {
			return current;
		}
	}

	return 0;
}

BOOL participants_serialize(ParticipantEntry *list, FILE *output) {
	LDEBUG("serializing participant list");
	SERIALIZE_LIST(
		jid_serialize(&list->jid, output) &&
		buffer_ptr_serialize(&list->nick, output) &&
		buffer_ptr_serialize(&list->presence, output) &&
		SERIALIZE_BASE(list->affiliation) &&
		SERIALIZE_BASE(list->role)
	);
	return TRUE;
}

BOOL participants_deserialize(struct ParticipantsList *participants, FILE *input, int limit) {
	int entry_count = 0;
	ParticipantEntry *new_entry = 0;
	ParticipantEntry **list = &participants->first;
	LDEBUG("deserializing participant list");
	DESERIALIZE_LIST(
		jid_deserialize(&new_entry->jid, input) &&
		buffer_ptr_deserialize(&new_entry->nick, input, MAX_JID_PART_SIZE) &&
		buffer_ptr_deserialize(&new_entry->presence, input, REASONABLE_RAW_LIMIT) &&
		DESERIALIZE_BASE(new_entry->affiliation) &&
		DESERIALIZE_BASE(new_entry->role),

		new_entry->next->prev = new_entry
	);
	participants->size = entry_count;
	return TRUE;
}

BOOL history_serialize(HistoryEntry *list, FILE *output) {
	LDEBUG("serializing room history");
	SERIALIZE_LIST(
		buffer_ptr_serialize(&list->nick, output) &&
		buffer_ptr_serialize(&list->header, output) &&
		buffer_ptr_serialize(&list->inner, output) &&
		SERIALIZE_BASE(list->delay)
	);
	return TRUE;
}

BOOL history_deserialize(struct HistoryList *history, FILE *input, int limit) {
	int entry_count = 0;
	HistoryEntry **list = &history->first, *new_entry = 0;
	LDEBUG("deserializing room history");
	DESERIALIZE_LIST(
		buffer_ptr_deserialize(&new_entry->nick, input, MAX_JID_PART_SIZE) &&
		buffer_ptr_deserialize(&new_entry->header, input, REASONABLE_RAW_LIMIT) &&
		buffer_ptr_deserialize(&new_entry->inner, input, REASONABLE_RAW_LIMIT) &&
		DESERIALIZE_BASE(new_entry->delay),

		new_entry->next->prev = new_entry
	);
	history->last = new_entry;
	history->size = entry_count;
	return TRUE;
}

BOOL affiliations_serialize(AffiliationEntry *list, FILE *output) {
	LDEBUG("serializing affiliations list");
	SERIALIZE_LIST(
		jid_serialize(&list->jid, output) &&
		buffer_serialize(&list->reason, output)
	);
	return TRUE;
}

BOOL affiliations_deserialize(AffiliationEntry **list, FILE *input, int limit) {
	int entry_count = 0;
	AffiliationEntry *new_entry = 0;
	LDEBUG("deserializing affiliations list");
	DESERIALIZE_LIST(
		jid_deserialize(&new_entry->jid, input) &&
		buffer_deserialize(&new_entry->reason, input, MAX_JID_PART_SIZE),
	);
	return TRUE;
}

static BOOL room_affiliation_add(Room *room, int sender_affiliation, int affiliation, Jid *jid) {
	int list;
	AffiliationEntry *affiliation_entry = 0,
					 *previous_affiliation_entry = 0;

	if (sender_affiliation != AFFIL_OWNER &&
			affiliation >= AFFIL_ADMIN) {
		return FALSE;
	}

	for (list = AFFIL_OUTCAST; list <= AFFIL_OWNER; ++list) {
		previous_affiliation_entry = 0;
		for (affiliation_entry = room->affiliations[list]; affiliation_entry;
				previous_affiliation_entry = affiliation_entry,
				affiliation_entry = affiliation_entry->next) {
			if (!jid_cmp(&affiliation_entry->jid, jid, JID_NODE | JID_HOST)) {
				break;
			}
		}
		if (affiliation_entry) {
			break;
		}
	}

	if (affiliation_entry) {
		if (sender_affiliation != AFFIL_OWNER &&
				list >= AFFIL_ADMIN) {
			return FALSE;
		}
		room_affiliation_detach_clear(&room->affiliations[list],
				affiliation_entry,
				previous_affiliation_entry);

		if (affiliation == AFFIL_NONE) {
			free(affiliation_entry);
		}
	}

	if (affiliation != AFFIL_NONE) {
		if (!affiliation_entry) {
			affiliation_entry = malloc(sizeof(*affiliation_entry));
		}
		jid_cpy(&affiliation_entry->jid, jid, JID_NODE | JID_HOST);
		affiliation_entry->reason.data = malloc(4);
		memcpy(affiliation_entry->reason.data, "test", 4);
		affiliation_entry->reason.size = 4;
		affiliation_entry->next = room->affiliations[affiliation];
		room->affiliations[affiliation] = affiliation_entry;
	}

	LDEBUG("set affiliation of '%.*s' to %d",
			JID_LEN(jid), JID_STR(jid),
			affiliation);

	return TRUE;
}

BOOL room_serialize(Room *room, FILE *output) {
	LDEBUG("serializing room '%.*s'", room->node.size, room->node.data);
	return
		buffer_serialize(&room->node, output) &&
		buffer_serialize(&room->title, output) &&
		buffer_serialize(&room->description, output) &&
		buffer_serialize(&room->password, output) &&

		buffer_ptr_serialize(&room->subject.data, output) &&
		buffer_ptr_serialize(&room->subject.nick, output) &&

		SERIALIZE_BASE(room->flags) &&
		SERIALIZE_BASE(room->default_role) &&
		SERIALIZE_BASE(room->participants.max_size) &&
		SERIALIZE_BASE(room->history.max_size) &&
		SERIALIZE_BASE(room->_unused) &&

		history_serialize(room->history.first, output) &&

		participants_serialize(room->participants.first, output) &&
		affiliations_serialize(room->affiliations[AFFIL_OWNER], output) &&
		affiliations_serialize(room->affiliations[AFFIL_ADMIN], output) &&
		affiliations_serialize(room->affiliations[AFFIL_MEMBER], output) &&
		affiliations_serialize(room->affiliations[AFFIL_OUTCAST], output);
}

BOOL room_deserialize(Room *room, FILE *input) {
	LDEBUG("deserializing room");
	return
		buffer_deserialize(&room->node, input, MAX_JID_PART_SIZE) &&
		buffer_deserialize(&room->title, input, USER_STRING_OPTION_LIMIT) &&
		buffer_deserialize(&room->description, input, USER_STRING_OPTION_LIMIT) &&
		buffer_deserialize(&room->password, input, USER_STRING_OPTION_LIMIT) &&

		buffer_ptr_deserialize(&room->subject.data, input, USER_STRING_OPTION_LIMIT) &&
		buffer_ptr_deserialize(&room->subject.nick, input, USER_STRING_OPTION_LIMIT) &&

		DESERIALIZE_BASE(room->flags) &&
		DESERIALIZE_BASE(room->default_role) &&
		DESERIALIZE_BASE(room->participants.max_size) &&
		DESERIALIZE_BASE(room->history.max_size) &&
		DESERIALIZE_BASE(room->_unused) &&

		history_deserialize(&room->history, input, HISTORY_ITEMS_COUNT_LIMIT) &&

		participants_deserialize(&room->participants, input, PARTICIPANTS_LIMIT) &&
		affiliations_deserialize(&room->affiliations[AFFIL_OWNER], input, AFFILIATION_LIST_LIMIT) &&
		affiliations_deserialize(&room->affiliations[AFFIL_ADMIN], input, AFFILIATION_LIST_LIMIT) &&
		affiliations_deserialize(&room->affiliations[AFFIL_MEMBER], input, AFFILIATION_LIST_LIMIT) &&
		affiliations_deserialize(&room->affiliations[AFFIL_OUTCAST], input, AFFILIATION_LIST_LIMIT);
}

static BOOL get_subject_node(BufferPtr *packet, BufferPtr *subject) {
	XmlNodeTraverser nodes = { .buffer = *packet };

	while (xmlfsm_traverse_node(&nodes)) {
		if (BUF_EQ_LIT("subject", &nodes.node_name)) {
			subject->data = nodes.node_start;
			subject->end = nodes.node.end;
			return TRUE;
		}
	}
	return FALSE;
}

static void send_to_participants(RouterChunk *chunk, ParticipantEntry *participant, int limit) {
	int sent = 0;
	for (; participant && sent < limit; participant = participant->next, ++sent) {
		LDEBUG("routing stanza to '%.*s', real JID '%.*s'",
				BPT_SIZE(&participant->nick), participant->nick.data,
				JID_LEN(&participant->jid), JID_STR(&participant->jid));
		chunk->egress.to = participant->jid;
		SEND(&chunk->send);
	}
}

BOOL room_attach_config_status_codes(Room *room, MucAdmNode *participant) {
	if (!(room->flags & MUC_FLAG_SEMIANONYMOUS)) {
		if (!builder_push_status_code(participant, STATUS_NON_ANONYMOUS)) {
			return FALSE;
		}
	}

	if (room->flags & MUC_FLAG_ENABLELOGGING) {
		if (!builder_push_status_code(participant, STATUS_LOGGING_ENABLED)) {
			return FALSE;
		}
	}

	return TRUE;
}

static BOOL room_broadcast_presence(Room *room, BuilderPacket *egress, SendCallback *send, ParticipantEntry *sender) {
	int orig_status_codes_count = egress->participant.status_codes_count;
	ParticipantEntry *receiver = room->participants.first;

	egress->name = 'p';
	egress->from_nick = sender->nick;
	egress->participant.affiliation = sender->affiliation;
	egress->participant.role = sender->role;

	for (; receiver; receiver = receiver->next) {
		if (receiver == sender) {
			if (!room_attach_config_status_codes(room, &egress->participant)) {
				LERROR("[BUG] unexpected status codes buffer overflow when trying to push room config");
				return FALSE;
			}
			builder_push_status_code(&egress->participant, STATUS_SELF_PRESENCE);
		} else {
			egress->participant.status_codes_count = orig_status_codes_count;
		}
		egress->to = receiver->jid;
		if (receiver->role == ROLE_MODERATOR || !(room->flags & MUC_FLAG_SEMIANONYMOUS)) {
			egress->participant.jid = &sender->jid;
		} else {
			egress->participant.jid = 0;
		}
		SEND(send);
	}
	return TRUE;
}

static void history_push(struct HistoryList *history, RouterChunk *chunk, BufferPtr *nick) {
	HistoryEntry *new_item = 0;

	while (history->size &&
			history->size >= history->max_size) {
		history_shift(history);
	}

	if (history->max_size <= 0) {
		return;
	}

	new_item = malloc(sizeof(*new_item));
	memset(new_item, 0, sizeof(*new_item));
	buffer_ptr_cpy(&new_item->nick, nick);
	buffer_ptr_cpy(&new_item->header, &chunk->ingress.header);
	buffer_ptr_cpy(&new_item->inner, &chunk->ingress.inner);
	new_item->delay = chunk->config->timer_thread.start +
		chunk->config->timer_thread.ticks / TIMER_RESOLUTION;

	if (history->first) {
		history->last->next = new_item;
		new_item->prev = history->last;
		history->last = new_item;
		++history->size;
	} else {
		history->first = history->last = new_item;
		history->size = 1;
	}
}

typedef struct {
	struct {
		int max_stanzas,
			max_chars,
			seconds;
	} history;
	BufferPtr password;
} MucNode;

static HistoryEntry *history_first_item(struct HistoryList *history, MucNode *muc_node) {
	HistoryEntry *first_item = 0;
	time_t now;
	if (muc_node->history.max_chars < 0 && muc_node->history.max_stanzas < 0 &&
			muc_node->history.seconds < 0) {
		// shortcut
		return history->first;
	}

	time(&now);
	for (first_item = history->last; first_item; first_item = first_item->prev) {
		if (
				(muc_node->history.max_chars >= 0 &&
				 (muc_node->history.max_chars -= BPT_SIZE(&first_item->inner)) < 0) ||
				(muc_node->history.max_stanzas >= 0 &&
				 !muc_node->history.max_stanzas--) ||
				(muc_node->history.seconds >= 0 &&
				 muc_node->history.seconds < difftime(now, first_item->delay))
		   ) {
			return first_item->next;
		}
	}
	return history->first;
}

static void history_send(HistoryEntry *history, RouterChunk *chunk, ParticipantEntry *receiver) {
	BuilderPacket *egress = &chunk->egress;

	egress->name = 'm';
	egress->type = 'g';
	egress->to = receiver->jid;

	for (; history; history = history->next) {
		egress->from_nick = history->nick;
		egress->header = history->header;
		egress->user_data = history->inner;
		egress->delay = history->delay;
		SEND(&chunk->send);
		egress->delay = 0;
	}
}

static void room_subject_send(Room *room, RouterChunk *chunk, ParticipantEntry *receiver) {
	BuilderPacket *egress = &chunk->egress;

	if (!BPT_NULL(&room->subject.data)) {
		egress->name = 'm';
		egress->type = 'g';
		egress->to = receiver->jid;
		egress->from_nick = room->subject.nick;
		BPT_INIT(&egress->header);
		egress->user_data = room->subject.data;
		SEND(&chunk->send);
	}
}

static void route_message(Room *room, RouterChunk *chunk) {
	IncomingPacket *ingress = &chunk->ingress;
	BuilderPacket *egress = &chunk->egress;
	ParticipantEntry *sender = 0, *receiver = 0;
	BufferPtr new_subject;

	if (!(sender = room_participant_by_jid(room, &ingress->real_from))) {
		router_error(chunk, &error_definitions[ERROR_EXTERNAL_MESSAGE]);
		return;
	}

	LDEBUG("user '%.*s', real JID '%.*s' is sending a message",
			BPT_SIZE(&sender->nick), sender->nick.data,
			JID_LEN(&sender->jid), JID_STR(&sender->jid));

	egress->from_nick = sender->nick;
	egress->user_data = ingress->inner;
	if (ingress->type == 'c') {
		if (!(receiver = room_participant_by_nick(room, &ingress->proxy_to.resource))) {
			router_error(chunk, &error_definitions[ERROR_RECIPIENT_NOT_IN_ROOM]);
			return;
		}

		if (sender->role == ROLE_VISITOR && !(room->flags & MUC_FLAG_VISITORSPM)) {
			router_error(chunk, &error_definitions[ERROR_NO_VISITORS_PM]);
			return;
		}

		if (!(room->flags & MUC_FLAG_ALLOWPM)) {
			router_error(chunk, &error_definitions[ERROR_NO_PM]);
			return;
		}

		LDEBUG("sending private message to '%.*s', real JID '%.*s'",
				BPT_SIZE(&receiver->nick), receiver->nick.data,
				JID_LEN(&receiver->jid), JID_STR(&receiver->jid));
		router_cleanup(ingress);
		send_to_participants(chunk, receiver, 1);
	} else if (ingress->type == 'g') {
		if (room->flags & MUC_FLAG_MODERATEDROOM && sender->role < ROLE_PARTICIPANT) {
			router_error(chunk, &error_definitions[ERROR_NO_VISITORS_PUBLIC]);
			return;
		}

		// TODO(artem): it is possible for the occupant to fabricate a <delay> in groupchat stanza;
		// by the time of writing this, ejabberd does not cut off that node - neither do we

		if (chunk->config->timer_thread.ticks - sender->last_message_time <
				chunk->config->worker.deciseconds_limit) {
			router_error(chunk, &error_definitions[ERROR_TRAFFIC_RATE]);
			return;
		}
		sender->last_message_time = chunk->config->timer_thread.ticks;

		if (get_subject_node(&ingress->inner, &new_subject)) {
			if (sender->role < ROLE_MODERATOR &&
					!(room->flags & MUC_FLAG_CHANGESUBJECT)) {
				router_error(chunk, &error_definitions[ERROR_PRIVILEGE_LEVEL]);
				return;
			}
			if (BPT_SIZE(&new_subject) > USER_STRING_OPTION_LIMIT) {
				router_error(chunk, &error_definitions[ERROR_IQ_BAD]);
				return;
			}
			free(room->subject.data.data);
			free(room->subject.nick.data);
			if (BPT_NULL(&new_subject)) {
				BPT_INIT(&room->subject.nick);
				BPT_INIT(&room->subject.data);
			} else {
				buffer_ptr_cpy(&room->subject.nick, &sender->nick);
				buffer_ptr_cpy(&room->subject.data, &new_subject);
			}
		}

		router_cleanup(ingress);
		history_push(&room->history, chunk, &sender->nick);
		send_to_participants(chunk, room->participants.first, 1 << 30);
	}
}

static int btoi(BufferPtr *buffer) {
	char int_buffer[21];
	int size = BPT_SIZE(buffer);
	if (size >= sizeof(int_buffer)) {
		return 0;
	} else {
		memcpy(int_buffer, buffer->data, size);
		int_buffer[size] = 0;
		return atoi(int_buffer);
	}
}

static void parse_muc_node(BufferPtr *buffer, MucNode *muc_node) {
	XmlNodeTraverser nodes = { .buffer = *buffer };
	XmlAttr attr;
	while (xmlfsm_traverse_node(&nodes)) {
		if (BUF_EQ_LIT("password", &nodes.node_name)) {
			if (xmlfsm_skip_attrs(&nodes.node)) {
				muc_node->password = nodes.node;
				muc_node->password.end -= sizeof("</password>") - 1;
			}
		} else if (BUF_EQ_LIT("history", &nodes.node_name)) {
			while (xmlfsm_get_attr(&nodes.node, &attr) == XMLPARSE_SUCCESS) {
				if (BPT_EQ_LIT("maxstanzas", &attr.name)) {
					muc_node->history.max_stanzas = btoi(&attr.value);
				} else if (BPT_EQ_LIT("maxchars", &attr.name)) {
					muc_node->history.max_chars = btoi(&attr.value);
				} else if (BPT_EQ_LIT("seconds", &attr.name)) {
					muc_node->history.seconds = btoi(&attr.value);
				}
			}
		}
	}
}

static BOOL fetch_muc_nodes(IncomingPacket *packet, MucNode *muc_node) {
	XmlNodeTraverser nodes = { .buffer = packet->inner };
	XmlAttr attr;
	int erase_index = 0;

	memset(muc_node, 0, sizeof(*muc_node));
	muc_node->history.max_chars =
		muc_node->history.max_stanzas =
		muc_node->history.seconds = -1;

	// First find the next free index of 'erase' block
	for (; erase_index < MAX_ERASE_CHUNKS; ++erase_index) {
		if (!packet->erase[erase_index].data) {
			break;
		}
	}

	while (xmlfsm_traverse_node(&nodes)) {
		if (BUF_EQ_LIT("x", &nodes.node_name)) {
			if (!xmlfsm_skipto_attr(&nodes.node, "xmlns", &attr)) {
				continue;
			}

			if (BPT_EQ_LIT("http://jabber.org/protocol/muc#user", &attr.value)) {
				LDEBUG("found a muc#user node, setting erase");
			} else if (BPT_EQ_LIT("http://jabber.org/protocol/muc", &attr.value)) {
				if (xmlfsm_skip_attrs(&nodes.node)) {
					parse_muc_node(&nodes.node, muc_node);
				}
			} else {
				continue;
			}

			if (erase_index == MAX_ERASE_CHUNKS) {
				LWARN("cannot allocate an erase chunk;"
						" muc node is not removable, thus considering stanza as invalid");
				return FALSE;
			}
			packet->erase[erase_index].data = nodes.node_start;
			packet->erase[erase_index].end = nodes.node.end;
			++erase_index;
		}
	}

	return TRUE;
}

static void route_presence(Room *room, RouterChunk *chunk) {
	IncomingPacket *ingress = &chunk->ingress;
	BuilderPacket *egress = &chunk->egress;
	ParticipantEntry *sender = 0, *receiver = 0;
	int affiliation;
	BufferPtr new_nick;
	BOOL just_joined = FALSE;
	MucNode muc_node;

	if (!fetch_muc_nodes(ingress, &muc_node)) {
		return; // stanza is obviously not valid, just drop silently
	}

	if ((sender = room_participant_by_jid(room, &ingress->real_from))) {
		if (sender->role == ROLE_VISITOR &&
				!(room->flags & MUC_FLAG_VISITORPRESENCE)) {
			// Visitors are not allowed to update presence
			router_error(chunk, &error_definitions[ERROR_NO_VISITORS_PRESENCE]);
			return;
		}

		// Sender is already a participant
		if (!BPT_NULL(&ingress->proxy_to.resource) &&
				(receiver = room_participant_by_nick(room, &ingress->proxy_to.resource)) != sender) {
			// Participant wants to change a nickname
			if (receiver) {
				router_error(chunk, &error_definitions[ERROR_OCCUPANT_CONFLICT]);
				return;
			}

			if (ingress->type != 'u') {
				// TODO(artem): check globally registered nickname

				buffer_ptr_cpy(&new_nick, &ingress->proxy_to.resource);
				egress->participant.nick = new_nick;
				egress->user_data = ingress->inner;
				egress->type = 'u';
				builder_push_status_code(&egress->participant, STATUS_NICKNAME_CHANGED);
				router_cleanup(ingress);

				room_broadcast_presence(room, egress, &chunk->send, sender);

				free(sender->nick.data);
				sender->nick = new_nick;
				egress->type = 0;
				egress->participant.nick.data =
					egress->participant.nick.end = 0;
			} else {
				router_cleanup(ingress);
			}
		} else {
			router_cleanup(ingress);
		}
	} else {
		// Participant not in room, but wants to join
		if (ingress->type == 'u') {
			// There's no use in 'unavailable' presence for the not-in-room user
			return;
		}
		
		if (BPT_NULL(&ingress->proxy_to.resource)) {
			router_error(chunk, &error_definitions[ERROR_JID_MALFORMED]);
			return;
		}

		if (room_participant_by_nick(room, &ingress->proxy_to.resource)) {
			router_error(chunk, &error_definitions[ERROR_OCCUPANT_CONFLICT]);
			return;
		}

		// TODO(artem): check globally registered nickname

		if (room->flags & MUC_FLAG_JUST_CREATED) {
			room_affiliation_add(room, AFFIL_OWNER, AFFIL_OWNER, &ingress->real_from);
			room->flags &= ~MUC_FLAG_JUST_CREATED;
			builder_push_status_code(&egress->participant, STATUS_ROOM_CREATED);
		}
		if (acl_role(chunk->acl, &ingress->real_from) >= ACL_MUC_ADMIN) {
			affiliation = AFFIL_OWNER;
		} else {
			if (room->participants.size >= room->participants.max_size) {
				router_error(chunk, &error_definitions[ERROR_OCCUPANTS_LIMIT]);
				return;
			}
			affiliation = room_get_affiliation(room, chunk->acl, &ingress->real_from);
			if (affiliation == AFFIL_OUTCAST) {
				// TODO(artem): show the reason of being banned?
				router_error(chunk, &error_definitions[ERROR_BANNED]);
				return;
			}
			if (affiliation < AFFIL_OWNER && room->flags & MUC_FLAG_PASSWORDPROTECTEDROOM) {
				if (!BUF_NULL(&room->password) &&
						(BPT_NULL(&muc_node.password) ||
						 !BPT_EQ_BIN(room->password.data, &muc_node.password, room->password.size))) {
					router_error(chunk, &error_definitions[ERROR_PASSWORD]);
					return;
				}
			}
			if (room->flags & MUC_FLAG_MEMBERSONLY && affiliation < AFFIL_MEMBER) {
				router_error(chunk, &error_definitions[ERROR_MEMBERS_ONLY]);
				return;
			}
		}

		receiver = room_join(room, &ingress->real_from, &ingress->proxy_to.resource, affiliation);
		router_cleanup(ingress);

		// Send occupants' presences to the new occupant. Skip the first item
		// as we know this is the one who just joined
		for (sender = room->participants.first->next; sender; sender = sender->next) {
			egress->from_nick = sender->nick;
			egress->participant.affiliation = sender->affiliation;
			egress->participant.role = sender->role;
			egress->user_data = sender->presence;
			if (receiver->role == ROLE_MODERATOR || !(room->flags & MUC_FLAG_SEMIANONYMOUS)) {
				egress->participant.jid = &sender->jid;
			} else {
				egress->participant.jid = 0;
			}
			send_to_participants(chunk, receiver, 1);
		}
		sender = receiver;
		just_joined = TRUE;
	}

	// Broadcast sender's presence to all participants
	egress->user_data = ingress->inner;
	// TODO(artem): set role = none if type = unavailable
	room_broadcast_presence(room, egress, &chunk->send, sender);

	if (just_joined) {
		history_send(history_first_item(&room->history, &muc_node), chunk, sender);
		room_subject_send(room, chunk, sender);
	}

	if (ingress->type == 'u') {
		room_leave(room, sender);
	} else {
		if (BPT_SIZE(&ingress->inner) <= REASONABLE_RAW_LIMIT) {
			// Cache participant's presence to broadcast it to newcomers
			if (sender->presence.data) {
				sender->presence.data = realloc(sender->presence.data, BPT_SIZE(&ingress->inner));
			} else {
				sender->presence.data = malloc(BPT_SIZE(&ingress->inner));
			}
			sender->presence.end = sender->presence.data + BPT_SIZE(&ingress->inner);
			memcpy(sender->presence.data, ingress->inner.data, BPT_SIZE(&ingress->inner));
		}
	}
}

static int affiliation_by_name(BufferPtr *name) {
	int affiliation;

	for (affiliation = AFFIL_OUTCAST; affiliation <= AFFIL_OWNER; ++affiliation) {
		if (BPT_EQ_BIN(affiliation_names[affiliation], name, affiliation_name_sizes[affiliation])) {
			return affiliation;
		}
	}
	return AFFIL_UNCHANGED;
}

static int role_by_name(BufferPtr *name) {
	int role;

	for (role = ROLE_NONE; role <= ROLE_MODERATOR; ++role) {
		if (BPT_EQ_BIN(role_names[role], name, role_name_sizes[role])) {
			return role;
		}
	}

	return ROLE_UNCHANGED;
}

static int next_muc_admin_item(BufferPtr *node, ParticipantEntry *target) {
	XmlAttr node_attr;
	XmlNodeTraverser nodes = { .buffer = *node };

	jid_init(&target->jid);
	target->role = ROLE_UNCHANGED;
	target->affiliation = AFFIL_UNCHANGED;
	BPT_INIT(&target->nick);

	while (xmlfsm_traverse_node(&nodes)) {
		if (!BUF_EQ_LIT("item", &nodes.node_name)) {
			continue;
		}

		while (xmlfsm_get_attr(&nodes.node, &node_attr) == XMLPARSE_SUCCESS) {
			if (BPT_EQ_LIT("affiliation", &node_attr.name)) {
				target->affiliation = affiliation_by_name(&node_attr.value);
				if (target->affiliation == AFFIL_UNCHANGED) {
					return -1;
				}
			} else if (BPT_EQ_LIT("role", &node_attr.name)) {
				target->role = role_by_name(&node_attr.value);
				if (target->role == ROLE_UNCHANGED) {
					return -1;
				}
			} else if (BPT_EQ_LIT("jid", &node_attr.name)) {
				if (!jid_struct(&node_attr.value, &target->jid)) {
					return -1;
				}
			} else if (BPT_EQ_LIT("nick", &node_attr.name)) {
				target->nick = node_attr.value;
			}
		}

		*node = nodes.buffer;
		return TRUE;
	}
	return FALSE;
}

static void push_affected_participant(ParticipantEntry **first, ParticipantEntry **current, ParticipantEntry *new) {
	if (!new->muc_admin_affected) {
		new->muc_admin_affected = TRUE;
		new->muc_admin_next_affected = 0;
		if (*first) {
			(*current)->muc_admin_next_affected = new;
			*current = new;
		} else {
			*first = *current = new;
		}
	}
}

static BOOL room_config_option_set(Room *room, BufferPtr *var, BufferPtr *value, int acl) {
	LDEBUG("got room config option '%.*s' = '%.*s' (%d)",
			BPT_SIZE(var), var->data,
			BPT_SIZE(value), value->data,
			btoi(value));

	if (BPT_EQ_LIT("muc#roomconfig_roomname", var)) {
		if (BPT_SIZE(value) <= USER_STRING_OPTION_LIMIT) {
			free(room->title.data);
			buffer__ptr_cpy(&room->title, value);
		}
	} else if (BPT_EQ_LIT("muc#roomconfig_roomdesc", var)) {
		if (BPT_SIZE(value) <= USER_STRING_OPTION_LIMIT) {
			free(room->description.data);
			buffer__ptr_cpy(&room->description, value);
		}
	} else if (BPT_EQ_LIT("muc#roomconfig_persistentroom", var)) {
		if (acl >= ACL_MUC_PERSIST) {
			if (btoi(value)) {
				room->flags |= MUC_FLAG_PERSISTENTROOM;
			} else {
				room->flags &= ~MUC_FLAG_PERSISTENTROOM;
			}
		}
	} else if (BPT_EQ_LIT("muc#roomconfig_publicroom", var)) {
		if (btoi(value)) {
			room->flags |= MUC_FLAG_PUBLICROOM;
		} else {
			room->flags &= ~MUC_FLAG_PUBLICROOM;
		}
	} else if (BPT_EQ_LIT("public_list", var)) {
		if (btoi(value)) {
			room->flags |= MUC_FLAG_PUBLICPARTICIPANTS;
		} else {
			room->flags &= ~MUC_FLAG_PUBLICPARTICIPANTS;
		}
	} else if (BPT_EQ_LIT("muc#roomconfig_passwordprotectedroom", var)) {
		if (BPT_SIZE(value) <= USER_STRING_OPTION_LIMIT) {
			if (btoi(value)) {
				room->flags |= MUC_FLAG_PASSWORDPROTECTEDROOM;
			} else {
				room->flags &= ~MUC_FLAG_PASSWORDPROTECTEDROOM;
			}
		}
	} else if (BPT_EQ_LIT("muc#roomconfig_roomsecret", var)) {
		free(room->password.data);
		buffer__ptr_cpy(&room->password, value);
	} else if (BPT_EQ_LIT("muc#roomconfig_maxusers", var)) {
		room->participants.max_size = btoi(value);
		if (room->participants.max_size <= 0 ||
				room->participants.max_size > PARTICIPANTS_LIMIT) {
			room->participants.max_size = PARTICIPANTS_LIMIT;
		}
	} else if (BPT_EQ_LIT("muc#roomconfig_whois", var)) {
		if (BPT_EQ_LIT("moderators", value)) {
			room->flags |= MUC_FLAG_SEMIANONYMOUS;
		} else {
			room->flags &= ~MUC_FLAG_SEMIANONYMOUS;
		}
	} else if (BPT_EQ_LIT("muc#roomconfig_membersonly", var)) {
		if (btoi(value)) {
			room->flags |= MUC_FLAG_MEMBERSONLY;
		} else {
			room->flags &= ~MUC_FLAG_MEMBERSONLY;
		}
	} else if (BPT_EQ_LIT("muc#roomconfig_moderatedroom", var)) {
		if (btoi(value)) {
			room->flags |= MUC_FLAG_MODERATEDROOM;
		} else {
			room->flags &= ~MUC_FLAG_MODERATEDROOM;
		}
	} else if (BPT_EQ_LIT("members_by_default", var)) {
		room->default_role = btoi(value) ? ROLE_PARTICIPANT : ROLE_VISITOR;
	} else if (BPT_EQ_LIT("muc#roomconfig_changesubject", var)) {
		if (btoi(value)) {
			room->flags |= MUC_FLAG_CHANGESUBJECT;
		} else {
			room->flags &= ~MUC_FLAG_CHANGESUBJECT;
		}
	} else if (BPT_EQ_LIT("allow_private_messages", var)) {
		if (btoi(value)) {
			room->flags |= MUC_FLAG_ALLOWPM;
		} else {
			room->flags &= ~MUC_FLAG_ALLOWPM;
		}
	} else if (BPT_EQ_LIT("allow_query_users", var)) {
		if (btoi(value)) {
			room->flags |= MUC_FLAG_IQ_PROXY;
		} else {
			room->flags &= ~MUC_FLAG_IQ_PROXY;
		}
	} else if (BPT_EQ_LIT("muc#roomconfig_allowinvites", var)) {
		if (btoi(value)) {
			room->flags |= MUC_FLAG_INVITES;
		} else {
			room->flags &= ~MUC_FLAG_INVITES;
		}
	} else if (BPT_EQ_LIT("muc#roomconfig_allowvisitorspm", var)) {
		if (btoi(value)) {
			room->flags |= MUC_FLAG_VISITORSPM;
		} else {
			room->flags &= ~MUC_FLAG_VISITORSPM;
		}
	} else if (BPT_EQ_LIT("muc#roomconfig_allowvisitorstatus", var)) {
		if (btoi(value)) {
			room->flags |= MUC_FLAG_VISITORPRESENCE;
		} else {
			room->flags &= ~MUC_FLAG_VISITORPRESENCE;
		}
	}
	return TRUE;
}

static BOOL room_config_parse(Room *room, BufferPtr *data, int acl) {
	XmlNodeTraverser field_nodes = { .buffer = *data }, value_nodes;
	XmlAttr var;
	while (xmlfsm_traverse_node(&field_nodes)) {
		if (!BUF_EQ_LIT("field", &field_nodes.node_name) ||
				!xmlfsm_skipto_attr(&field_nodes.node, "var", &var) ||
				!xmlfsm_skip_attrs(&field_nodes.node)) {
			continue;
		}
		value_nodes.buffer = field_nodes.node;
		while (xmlfsm_traverse_node(&value_nodes)) {
			if (!BUF_EQ_LIT("value", &value_nodes.node_name) ||
					!xmlfsm_skip_attrs(&value_nodes.node)) {
				continue;
			}
			value_nodes.node.end -= sizeof("</value>") - 1;
			if (!room_config_option_set(room, &var.value, &value_nodes.node, acl)) {
				return FALSE;
			}
		}
	}
	return TRUE;
}

static void route_iq(Room *room, RouterChunk *chunk) {
	IncomingPacket *ingress = &chunk->ingress;
	BuilderPacket *egress = &chunk->egress;
	ParticipantEntry *sender = 0, *receiver = 0, target,
					 *first_affected_participant = 0,
					 *current_affected_participant = 0;

	BufferPtr xmlns = BPT_INITIALIZER;
	XmlAttr node_attr;
	XmlNodeTraverser nodes = { .buffer = ingress->inner };
	int admin_state;
	BOOL is_query_node_empty;

	sender = room_participant_by_jid(room, &ingress->real_from);
	if (!BPT_NULL(&ingress->proxy_to.resource)) {
		// iq directed to participant - just proxying
		if (!sender) {
			router_error(chunk, &error_definitions[ERROR_EXTERNAL_IQ]);
			return;
		}
		if (!(room->flags & MUC_FLAG_IQ_PROXY)) {
			router_error(chunk, &error_definitions[ERROR_IQ_PROHIBITED]);
			return;
		}
		if (!(receiver = room_participant_by_nick(room, &ingress->proxy_to.resource))) {
			router_error(chunk, &error_definitions[ERROR_RECIPIENT_NOT_IN_ROOM]);
			return;
		}

		router_cleanup(ingress);

		egress->from_nick = sender->nick;
		egress->user_data = ingress->inner;
		egress->to = receiver->jid;

		// special routing for vcard request - 'to' should not contain resource
		if (ingress->type == 'g') {
			nodes.buffer = ingress->inner;
			if (xmlfsm_traverse_node(&nodes)) {
				if (BUF_EQ_LIT("vCard", &nodes.node_name)) {
					BPT_INIT(&egress->to.resource);
				}
			}
		}

		SEND(&chunk->send);
		return;
	}

	egress->type = 'r';

	// first find <query> node and it's xmlns
	while (xmlfsm_traverse_node(&nodes)) {
		if (BUF_EQ_LIT("query", &nodes.node_name)) {
			if (xmlfsm_skipto_attr(&nodes.node, "xmlns", &node_attr)) {
				xmlns = node_attr.value;
				is_query_node_empty = !xmlfsm_skip_attrs(&nodes.node);
				break;
			}
		}
	}

	if (BPT_NULL(&xmlns)) {
		router_error(chunk, &error_definitions[ERROR_IQ_BAD]);
		return;
	}

	LDEBUG("received iq:%c, xmlns '%.*s'", ingress->type, BPT_SIZE(&xmlns), xmlns.data);
	if (ingress->type == 'g') {
		if (BPT_EQ_LIT("http://jabber.org/protocol/disco#info", &xmlns)) {
			egress->iq_type = BUILD_IQ_ROOM_DISCO_INFO;
			egress->room = room;
		} else if (BPT_EQ_LIT("http://jabber.org/protocol/disco#items", &xmlns)) {
			if (room->flags & MUC_FLAG_PUBLICPARTICIPANTS) {
				egress->iq_type = BUILD_IQ_ROOM_DISCO_ITEMS;
				egress->room = room;
			} else {
				router_error(chunk, &error_definitions[ERROR_NOT_IMPLEMENTED]);
				return;
			}
		} else if (BPT_EQ_LIT("http://jabber.org/protocol/muc#admin", &xmlns)) {
			if (is_query_node_empty) {
				router_error(chunk, &error_definitions[ERROR_IQ_BAD]);
				return;
			}

			if (!sender) {
				router_error(chunk, &error_definitions[ERROR_EXTERNAL_IQ]);
				return;
			}
			if (sender->affiliation < AFFIL_ADMIN) {
				router_error(chunk, &error_definitions[ERROR_PRIVILEGE_LEVEL]);
				return;
			}

			if (next_muc_admin_item(&nodes.node, &target) <= 0 ||
					target.affiliation == AFFIL_UNCHANGED ||
					target.affiliation == AFFIL_NONE) {
				router_error(chunk, &error_definitions[ERROR_IQ_BAD]);
				return;
			}

			egress->iq_type = BUILD_IQ_ROOM_AFFILIATIONS;
			egress->muc_items.affiliation = target.affiliation;
			egress->muc_items.items = room->affiliations[egress->muc_items.affiliation];
		} else if (BPT_EQ_LIT("http://jabber.org/protocol/muc#owner", &xmlns)) {
			if  (!sender) {
				router_error(chunk, &error_definitions[ERROR_EXTERNAL_IQ]);
				return;
			}
			if (sender->affiliation < AFFIL_OWNER) {
				router_error(chunk, &error_definitions[ERROR_PRIVILEGE_LEVEL]);
				return;
			}

			egress->iq_type = BUILD_IQ_ROOM_CONFIG;
			egress->room = room;
		}
	} else if (ingress->type == 's') {
		if (BPT_EQ_LIT("http://jabber.org/protocol/muc#admin", &xmlns)) {
			if (is_query_node_empty) {
				router_error(chunk, &error_definitions[ERROR_IQ_BAD]);
				return;
			}

			if (!sender) {
				router_error(chunk, &error_definitions[ERROR_EXTERNAL_IQ]);
				return;
			}

			if (sender->role < ROLE_MODERATOR) {
				router_error(chunk, &error_definitions[ERROR_PRIVILEGE_LEVEL]);
				return;
			}

			while ((admin_state = next_muc_admin_item(&nodes.node, &target))) {
				if (admin_state < 0) {
					router_error(chunk, &error_definitions[ERROR_IQ_BAD]);
					return;
				}
				if ((target.role != ROLE_UNCHANGED && BPT_BLANK(&target.nick)) ||
						(target.affiliation != AFFIL_UNCHANGED && JID_EMPTY(&target.jid)) ||
						((target.role == ROLE_UNCHANGED) == (target.affiliation == AFFIL_UNCHANGED))) {
					router_error(chunk, &error_definitions[ERROR_IQ_BAD]);
					return;
				}

				if (target.role != ROLE_UNCHANGED) {
					receiver = room_participant_by_nick(room, &target.nick);
					if (!receiver) {
						router_error(chunk, &error_definitions[ERROR_RECIPIENT_NOT_IN_ROOM]);
						return;
					}
					if (receiver->affiliation >= AFFIL_ADMIN || // no one can change role of admin+
							(sender->affiliation < AFFIL_ADMIN && // if just a moderator,
							 (receiver->role >= ROLE_MODERATOR || // may only change roles of non-moderators
							  target.role >= ROLE_MODERATOR))) { // to non-moderators
						router_error(chunk, &error_definitions[ERROR_PRIVILEGE_LEVEL]);
						return;
					}
					receiver->role = target.role;

					push_affected_participant(&first_affected_participant,
							&current_affected_participant, receiver);
				}

				if (target.affiliation != AFFIL_UNCHANGED) {
					if (sender->affiliation < AFFIL_ADMIN) {
						router_error(chunk, &error_definitions[ERROR_PRIVILEGE_LEVEL]);
						return;
					}

					if (!room_affiliation_add(room, sender->affiliation, target.affiliation, &target.jid)) {
						router_error(chunk, &error_definitions[ERROR_PRIVILEGE_LEVEL]);
						return;
					}

					for (receiver = room->participants.first; receiver; receiver = receiver->next) {
						target.affiliation = room_get_affiliation(room, chunk->acl, &receiver->jid);
						if (target.affiliation != receiver->affiliation) {
							receiver->role =
								(target.affiliation >= AFFIL_ADMIN) ? ROLE_MODERATOR :
								(target.affiliation == AFFIL_OUTCAST) ? ROLE_NONE :
								ROLE_PARTICIPANT;
							receiver->affiliation = target.affiliation;
							push_affected_participant(&first_affected_participant,
									&current_affected_participant, receiver);
						}
					}
				}
			}

			egress->iq_type = BUILD_IQ_ROOM_AFFILIATIONS;
			egress->muc_items.items = 0;
		} else if (BPT_EQ_LIT("http://jabber.org/protocol/muc#owner", &xmlns)) {
			if (is_query_node_empty) {
				router_error(chunk, &error_definitions[ERROR_IQ_BAD]);
				return;
			}

			if (!sender) {
				router_error(chunk, &error_definitions[ERROR_EXTERNAL_IQ]);
				return;
			}
			if (sender->affiliation < AFFIL_OWNER) {
				router_error(chunk, &error_definitions[ERROR_PRIVILEGE_LEVEL]);
				return;
			}

			nodes.buffer = nodes.node;
			while (xmlfsm_traverse_node(&nodes)) {
				if (BUF_EQ_LIT("destroy", &nodes.node_name)) {
					room->flags |= MUC_FLAG_DESTROYED;
					for (receiver = room->participants.first; receiver; receiver = receiver->next) {
						receiver->role = ROLE_NONE;
						receiver->affiliation = AFFIL_NONE;
						push_affected_participant(&first_affected_participant,
								&current_affected_participant, receiver);
					}
					egress->participant.destroy_node.data = nodes.node_start;
					egress->participant.destroy_node.end = nodes.node.end;
					egress->iq_type = BUILD_IQ_EMPTY;
					break;
				} else if (BUF_EQ_LIT("x", &nodes.node_name)) {
					if (xmlfsm_skip_attrs(&nodes.node)) {
						if (!room_config_parse(room, &nodes.node, acl_role(&chunk->config->acl_config, &sender->jid))) {
							router_error(chunk, &error_definitions[ERROR_IQ_BAD]);
							return;
						}
					}
					if (room->flags & MUC_FLAG_MEMBERSONLY) {
						for (receiver = room->participants.first; receiver; receiver = receiver->next) {
							if (receiver->affiliation < AFFIL_MEMBER) {
								receiver->role = ROLE_NONE;
								push_affected_participant(&first_affected_participant,
										&current_affected_participant, receiver);
							}
						}
					}
					egress->iq_type = BUILD_IQ_EMPTY;
					break;
				}
			}
		}
	}

	if (egress->iq_type) {
		// XXX(artem): use 'from' => '  to' substitution to avoid JID copying!
		// TODO(artem): optimization note: use sender->jid when available
		jid_cpy(&egress->to, &ingress->real_from, JID_FULL);
		router_cleanup(ingress);
		SEND(&chunk->send);
		jid_destroy(&egress->to);
	} else {
		router_error(chunk, &error_definitions[ERROR_NOT_IMPLEMENTED]);
		return;
	}

	for (current_affected_participant = first_affected_participant;
			current_affected_participant; ) {
		current_affected_participant->muc_admin_affected = FALSE;
		egress->participant.status_codes_count = 0;
		egress->type = '\0';
		if (room->flags & MUC_FLAG_DESTROYED) {
			egress->type = 'u';
		} else if (current_affected_participant->affiliation == AFFIL_OUTCAST) {
			builder_push_status_code(&egress->participant, STATUS_BANNED);
			egress->type = 'u';
		} else if (room->flags & MUC_FLAG_MEMBERSONLY &&
				current_affected_participant->affiliation < AFFIL_MEMBER) {
			builder_push_status_code(&egress->participant, STATUS_NONMEMBER_REMOVED);
			egress->type = 'u';
		} else if (current_affected_participant->role == ROLE_NONE) {
			builder_push_status_code(&egress->participant, STATUS_KICKED);
			egress->type = 'u';
		}

		if (egress->type == 'u') {
			BPT_INIT(&egress->user_data);
		} else {
			egress->user_data = current_affected_participant->presence;
		}

		room_broadcast_presence(room, egress, &chunk->send, current_affected_participant);
		if (current_affected_participant->role == ROLE_NONE) {
			first_affected_participant = current_affected_participant->muc_admin_next_affected;
			room_leave(room, current_affected_participant);
			current_affected_participant = first_affected_participant;
		} else {
			current_affected_participant = current_affected_participant->muc_admin_next_affected;
		}
	}
}

void room_route(Room *room, RouterChunk *chunk) {
	ParticipantEntry *sender;
	if (chunk->ingress.type == 'e' && chunk->ingress.name != 'i') {
		// <message> or <presence> with type='error'
		if ((sender = room_participant_by_jid(room, &chunk->ingress.real_from))) {
			BPT_SET_LIT(&chunk->egress.user_data,
					"<status>This occupant is kicked from the room because he sent an error stanza</status>");
			chunk->egress.type = 'u';
			sender->role = ROLE_NONE;
			router_cleanup(&chunk->ingress);
			room_broadcast_presence(room, &chunk->egress, &chunk->send, sender);
			room_leave(room, sender);
		}
		return;
	}
	switch (chunk->ingress.name) {
		case 'm':
			route_message(room, chunk);
			break;
		case 'p':
			route_presence(room, chunk);
			break;
		case 'i':
			route_iq(room, chunk);
			break;
	}
}
