#include <stdlib.h>
#include <string.h>

#include "xmcomp/src/logger.h"

#include "serializer.h"
#ifdef MEWCAT
#	include "mewcat.h"
#endif
#include "room.h"
#include "timer.h"
#include "worker.h"

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

void room_init(Room *room, BufferPtr *node) {
	memset(room, 0, sizeof(*room));
	pthread_mutex_init(&room->sync, 0);
	buffer__ptr_cpy(&room->node, node);
	room->flags = MUC_FLAGS_DEFAULT;
	room->participants.max_size = 100;
	room->history.max_size = 20;
	room->default_role = ROLE_PARTICIPANT;
}

void room_destroy(Room *room) {
	int i;

	LDEBUG("destroying room '%.*s'", room->node.size, room->node.data);
	DLS_CLEAR(&room->participants, participant_destroy, Participant);
	free(room->node.data);
	free(room->title.data);
	free(room->description.data);
	free(room->subject.node.data);
	free(room->subject.nick.data);
	free(room->password.data);
	for (i = AFFIL_OUTCAST; i <= AFFIL_OWNER; ++i) {
		DLS_CLEAR(&room->affiliations[i], affiliation_destroy, Affiliation);
	}
	DLS_CLEAR(&room->history, history_entry_destroy, HistoryEntry);
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

Participant *room_join(Room *room, Jid *jid, BufferPtr *nick, int affiliation) {
	LDEBUG("'%.*s' is joining the room '%.*s' as '%.*s'",
			JID_LEN(jid), JID_STR(jid),
			room->node.size, room->node.data,
			BPT_SIZE(nick), nick->data);

	Participant *participant = participant_init(malloc(sizeof(*participant)), jid);
	participant_set_nick(participant, nick);

	DLS_PREPEND(&room->participants, participant);

	participant->affiliation = affiliation;
	participant->role = room_role_for_affiliation(room, affiliation);

	LDEBUG("created new participant #%d, role %d affil %d",
			room->participants.size, participant->role, participant->affiliation);

	return participant;
}

void room_leave(Room *room, Participant *participant) {
	LDEBUG("participant #%d '%.*s' (JID '%.*s') is leaving the room '%.*s'",
			room->participants.size,
			BPT_SIZE(&participant->nick), participant->nick.data,
			JID_LEN(&participant->jid), JID_STR(&participant->jid),
			room->node.size, room->node.data);

	DLS_DELETE(&room->participants, participant);
	free(participant_destroy(participant));

	if (!(room->flags & MUC_FLAG_PERSISTENTROOM) &&
			!room->participants.size) {
		room->flags |= MUC_FLAG_DESTROYED;
	}
}

BOOL room_serialize(Room *room, FILE *output) {
	LDEBUG("serializing room '%.*s'", room->node.size, room->node.data);
	return
		buffer_serialize(&room->node, output) &&
		buffer_serialize(&room->title, output) &&
		buffer_serialize(&room->description, output) &&
		buffer_serialize(&room->password, output) &&

		buffer_ptr_serialize(&room->subject.node, output) &&
		buffer_ptr_serialize(&room->subject.nick, output) &&

		SERIALIZE_BASE(room->flags) &&
		SERIALIZE_BASE(room->default_role) &&
		SERIALIZE_BASE(room->participants.max_size) &&
		SERIALIZE_BASE(room->history.max_size) &&
		SERIALIZE_BASE(room->_unused) &&

		history_entries_serialize(&room->history, output) &&

		participants_serialize(&room->participants, output) &&
		affiliations_serialize(&room->affiliations[AFFIL_OWNER], output) &&
		affiliations_serialize(&room->affiliations[AFFIL_ADMIN], output) &&
		affiliations_serialize(&room->affiliations[AFFIL_MEMBER], output) &&
		affiliations_serialize(&room->affiliations[AFFIL_OUTCAST], output);
}

BOOL room_deserialize(Room *room, FILE *input) {
	LDEBUG("deserializing room");
	for (int affiliation = AFFIL_OUTCAST; affiliation <= AFFIL_OWNER; ++affiliation) {
		room->affiliations[affiliation].max_size = 10240;
	}
	return
		buffer_deserialize(&room->node, input, MAX_JID_PART_SIZE) &&
		buffer_deserialize(&room->title, input, USER_STRING_OPTION_LIMIT) &&
		buffer_deserialize(&room->description, input, USER_STRING_OPTION_LIMIT) &&
		buffer_deserialize(&room->password, input, USER_STRING_OPTION_LIMIT) &&

		buffer_ptr_deserialize(&room->subject.node, input, USER_STRING_OPTION_LIMIT) &&
		buffer_ptr_deserialize(&room->subject.nick, input, USER_STRING_OPTION_LIMIT) &&

		DESERIALIZE_BASE(room->flags) &&
		DESERIALIZE_BASE(room->default_role) &&
		DESERIALIZE_BASE(room->participants.max_size) &&
		DESERIALIZE_BASE(room->history.max_size) &&
		DESERIALIZE_BASE(room->_unused) &&

		history_entries_deserialize(&room->history, input) &&

		participants_deserialize(&room->participants, input) &&
		affiliations_deserialize(&room->affiliations[AFFIL_OWNER], input) &&
		affiliations_deserialize(&room->affiliations[AFFIL_ADMIN], input) &&
		affiliations_deserialize(&room->affiliations[AFFIL_MEMBER], input) &&
		affiliations_deserialize(&room->affiliations[AFFIL_OUTCAST], input);
}

static BOOL get_subject_node(BufferPtr *packet, BufferPtr *subject) {
	XmlNodeTraverser nodes = { .buffer = *packet };

	while (xmlfsm_next_sibling(&nodes)) {
		if (BUF_EQ_LIT("subject", &nodes.node_name)) {
			subject->data = nodes.node_start;
			subject->end = nodes.node.end;
			return TRUE;
		}
	}
	return FALSE;
}

static inline int room_config_status_codes(Room *room) {
	int codes = 0;

	if (!(room->flags & MUC_FLAG_SEMIANONYMOUS)) {
		codes |= STATUS_NON_ANONYMOUS;
	}

	if (room->flags & MUC_FLAG_ENABLELOGGING) {
		codes |= STATUS_LOGGING_ENABLED;
	}

	if (room->flags & MUC_FLAG_JUST_CREATED) {
		codes |= STATUS_ROOM_CREATED;
	}

	return codes;
}

static BOOL room_broadcast_presence(Room *room, Participant *sender, BufferPtr *new_nick, int status_codes) {
	BuilderPacket egress = {};

	egress.name = STANZA_PRESENCE;

	if (sender->role == ROLE_NONE || (status_codes & STATUS_NICKNAME_CHANGED)) {
		egress.type = STANZA_PRESENCE_UNAVAILABLE;
	}

	if (new_nick) {
		egress.sys_data.presence.item.nick = *new_nick;
	}

	egress.user_data = sender->presence;
	egress.from_nick = sender->nick;
	egress.from_node = room->node;
	egress.sys_data.presence.item.affiliation = sender->affiliation;
	egress.sys_data.presence.item.role = sender->role;
	egress.sys_data.presence.item.reason_node = sender->affected_list.reason_node;

	Participant *receiver = 0;
	DLS_FOREACH(&room->participants, receiver) {
		egress.to = receiver->jid;

		// Set 'self-presence' code (and include room codes).
		if (receiver == sender) {
			egress.sys_data.presence.status_codes = room_config_status_codes(room) | STATUS_SELF_PRESENCE | status_codes;
		} else {
			egress.sys_data.presence.status_codes = status_codes;
		}

		// Attach JID.
		if (receiver->role == ROLE_MODERATOR || !(room->flags & MUC_FLAG_SEMIANONYMOUS)) {
			egress.sys_data.presence.item.jid = sender->jid;
		} else {
			jid_init(&egress.sys_data.presence.item.jid);
		}

#ifdef MEWCAT
		if (!mewcat_handle(room, sender, receiver, &egress)) {
			continue;
		}
#endif
		worker_send(&egress);
	}
	return TRUE;
}

typedef struct {
	struct {
		int max_stanzas,
			max_chars,
			seconds;
	} history;
	BufferPtr password;
} MucNode;

static void room_subject_send(Room *room, Participant *receiver) {
	BuilderPacket egress = {};

	if (!BPT_NULL(&room->subject.node)) {
		egress.name = STANZA_MESSAGE;
		egress.type = STANZA_MESSAGE_GROUPCHAT;
		egress.to = receiver->jid;
		egress.from_nick = room->subject.nick;
		egress.from_node = room->node;
		egress.user_data = room->subject.node;
		worker_send(&egress);
	}
}

static void parse_muc_node(BufferPtr *buffer, MucNode *muc_node) {
	XmlNodeTraverser nodes = { .buffer = *buffer };
	XmlAttr attr;
	Buffer _node_name;

	muc_node->history.max_stanzas = -1;
	muc_node->history.max_chars = -1;
	muc_node->history.seconds = -1;
	BPT_INIT(&muc_node->password);

	if (BPT_BLANK(&nodes.buffer)) {
		// No muc node.
		return;
	}

	xmlfsm_node_name(&nodes.buffer, &_node_name);
	if (!xmlfsm_skip_attrs(&nodes.buffer)) {
		// Empty node.
		return;
	}

	LDEBUG("parsing muc node: '%.*s'", BPT_SIZE(buffer), buffer->data);
	while (xmlfsm_next_sibling(&nodes)) {
		if (BUF_EQ_LIT("password", &nodes.node_name)) {
			if (xmlfsm_skip_attrs(&nodes.node)) {
				muc_node->password = nodes.node;
				muc_node->password.end -= sizeof("</password>") - 1;
			}
		} else if (BUF_EQ_LIT("history", &nodes.node_name)) {
			while (xmlfsm_next_attr(&nodes.node, &attr) == XMLPARSE_SUCCESS) {
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

static void route_message(Room *room, IncomingPacket *ingress, int deciseconds_limit) {
	Participant *sender = 0;
	if (!(sender = participants_find_by_jid(&room->participants, &ingress->real_from))) {
		worker_bounce(ingress, &error_definitions[ERROR_EXTERNAL_MESSAGE], &room->node);
		return;
	}

	LDEBUG("user '%.*s', real JID '%.*s' is sending a message",
			BPT_SIZE(&sender->nick), sender->nick.data,
			JID_LEN(&sender->jid), JID_STR(&sender->jid));

	BuilderPacket egress = {
		.name = STANZA_MESSAGE,
		.type = ingress->type,
		.header = ingress->header,
		.from_node = room->node,
		.from_nick = sender->nick,
		.user_data = ingress->inner
	};

	if (BPT_NULL(&ingress->proxy_to.resource)) {
		if (room->flags & MUC_FLAG_MODERATEDROOM && sender->role < ROLE_PARTICIPANT) {
			worker_bounce(ingress, &error_definitions[ERROR_NO_VISITORS_PUBLIC], &room->node);
			return;
		}

		// TODO(artem): it is possible for the occupant to fabricate a <delay> in groupchat stanza;
		// by the time of writing this, ejabberd does not cut off that node - neither do we

		if (timer_ticks() - sender->last_message_time < deciseconds_limit) {
			worker_bounce(ingress, &error_definitions[ERROR_TRAFFIC_RATE], &room->node);
			return;
		}

#ifdef MEWCAT
		if (!mewcat_handle(room, sender, 0, &egress)) {
			return;
		}
#endif

		sender->last_message_time = timer_ticks();

		BufferPtr new_subject;
		if (get_subject_node(&ingress->inner, &new_subject)) {
			if (sender->role < ROLE_MODERATOR &&
					!(room->flags & MUC_FLAG_CHANGESUBJECT)) {
				worker_bounce(ingress, &error_definitions[ERROR_PRIVILEGE_LEVEL], &room->node);
				return;
			}
			if (BPT_SIZE(&new_subject) > USER_STRING_OPTION_LIMIT) {
				worker_bounce(ingress, &error_definitions[ERROR_IQ_BAD], &room->node);
				return;
			}
			free(room->subject.node.data);
			free(room->subject.nick.data);
			if (BPT_NULL(&new_subject)) {
				BPT_INIT(&room->subject.nick);
				BPT_INIT(&room->subject.node);
			} else {
				buffer_ptr_cpy(&room->subject.nick, &sender->nick);
				buffer_ptr_cpy(&room->subject.node, &new_subject);
			}
		}

		packet_cleanup(ingress, PACKET_CLEANUP_FROM | PACKET_CLEANUP_TO);
		HistoryEntry *history_entry = history_entries_push(&room->history);
		history_entry_set_inner(history_entry, &egress.user_data);
		// TODO(artem): set history_entry props
		Participant *receiver = 0;
		DLS_FOREACH(&room->participants, receiver) {
			egress.to = receiver->jid;
#ifdef MEWCAT
			if (!mewcat_handle(room, sender, receiver, &egress)) {
				continue;
			}
#endif
			worker_send(&egress);
		}
	} else {
		Participant *receiver = 0;
		if (!(receiver = participants_find_by_nick(&room->participants, &ingress->proxy_to.resource))) {
			worker_bounce(ingress, &error_definitions[ERROR_RECIPIENT_NOT_IN_ROOM], &room->node);
			return;
		}

		if (sender->role == ROLE_VISITOR && !(room->flags & MUC_FLAG_VISITORSPM)) {
			worker_bounce(ingress, &error_definitions[ERROR_NO_VISITORS_PM], &room->node);
			return;
		}

		if (!(room->flags & MUC_FLAG_ALLOWPM)) {
			worker_bounce(ingress, &error_definitions[ERROR_NO_PM], &room->node);
			return;
		}

#ifdef MEWCAT
		if (!mewcat_handle(room, sender, receiver, &egress)) {
			return;
		}
#endif

		packet_cleanup(ingress, PACKET_CLEANUP_FROM | PACKET_CLEANUP_TO);
		egress.to = receiver->jid;
		worker_send(&egress);
	}
}

static void room_presences_send(Room *room, Participant *receiver) {
	BuilderPacket egress = {
		.name = STANZA_PRESENCE,
		.from_node = room->node
	};

	Participant *sender = 0;
	DLS_FOREACH(&room->participants, sender) {
		if (sender != receiver) {
			egress.from_nick = sender->nick;
			egress.sys_data.presence.item.affiliation = sender->affiliation;
			egress.sys_data.presence.item.role = sender->role;
			egress.user_data = sender->presence;
			if (receiver->role == ROLE_MODERATOR || !(room->flags & MUC_FLAG_SEMIANONYMOUS)) {
				egress.sys_data.presence.item.jid = sender->jid;
			} else {
				jid_init(&egress.sys_data.presence.item.jid);
			}
			egress.to = receiver->jid;
			worker_send(&egress);
		}
	}
}

static void route_presence(Room *room, IncomingPacket *ingress, ACLConfig *acl) {
	Participant *sender = 0;
	BOOL just_joined = FALSE;

	MucNode muc_node;
	parse_muc_node(&ingress->inner_nodes.presence.muc, &muc_node);

	LDEBUG("muc_node:\n"
			"\tmax_stanzas: %d\n"
			"\tmax_chars: %d\n"
			"\tseconds: %d\n"
			"\tpassword: %d",
			muc_node.history.max_stanzas, muc_node.history.max_chars, muc_node.history.seconds, BPT_SIZE(&muc_node.password));

	if ((sender = participants_find_by_jid(&room->participants, &ingress->real_from))) {
		if (ingress->type != STANZA_PRESENCE_UNAVAILABLE && !BPT_NULL(&ingress->proxy_to.resource)) {
			Participant *receiver = participants_find_by_nick(&room->participants, &ingress->proxy_to.resource);
			if (sender == receiver) {
				packet_cleanup(ingress, PACKET_CLEANUP_FROM | PACKET_CLEANUP_TO);
			} else if (!receiver) {
				if (sender->role <= ROLE_VISITOR && !(room->flags & MUC_FLAG_VISITORPRESENCE)) {
					// Ignore nick changes from visitors
					return;
				}

				// TODO(artem): check globally registered nickname
				BufferPtr new_nick;
				buffer_ptr_cpy(&new_nick, &ingress->proxy_to.resource);
				room_broadcast_presence(room, sender, &new_nick, STATUS_NICKNAME_CHANGED);
				free(sender->nick.data);
				sender->nick = new_nick;
			} else {
				worker_bounce(ingress, &error_definitions[ERROR_OCCUPANT_CONFLICT], &room->node);
				return;
			}
		} else {
			packet_cleanup(ingress, PACKET_CLEANUP_FROM | PACKET_CLEANUP_TO);
		}
	} else {
		// Participant not in room, but wants to join
		if (ingress->type == STANZA_PRESENCE_UNAVAILABLE) {
			// There's no use in 'unavailable' presence for the not-in-room user
			return;
		}
		
		if (BPT_NULL(&ingress->proxy_to.resource)) {
			worker_bounce(ingress, &error_definitions[ERROR_JID_MALFORMED], &room->node);
			return;
		}

		if (participants_find_by_nick(&room->participants, &ingress->proxy_to.resource)) {
			worker_bounce(ingress, &error_definitions[ERROR_OCCUPANT_CONFLICT], &room->node);
			return;
		}

		// TODO(artem): check globally registered nickname

		if (room->flags & MUC_FLAG_JUST_CREATED) {
			affiliationss_add(room->affiliations, 0, AFFIL_OWNER, &ingress->real_from, 0);
		}

		int affiliation;
		if (acl_role(acl, &ingress->real_from) >= ACL_MUC_ADMIN) {
			affiliation = AFFIL_OWNER;
		} else {
			if (room->participants.size >= room->participants.max_size) {
				worker_bounce(ingress, &error_definitions[ERROR_OCCUPANTS_LIMIT], &room->node);
				return;
			}
			affiliation = affiliationss_get_by_jid((AffiliationsList **)room->affiliations, acl, &ingress->real_from);
			if (affiliation == AFFIL_OUTCAST) {
				// TODO(artem): show the reason of being banned?
				worker_bounce(ingress, &error_definitions[ERROR_BANNED], &room->node);
				return;
			}
			if (affiliation < AFFIL_OWNER && room->flags & MUC_FLAG_PASSWORDPROTECTEDROOM) {
				if (!BUF_NULL(&room->password) &&
						(BPT_NULL(&muc_node.password) ||
						 !BPT_EQ_BIN(room->password.data, &muc_node.password, room->password.size))) {
					worker_bounce(ingress, &error_definitions[ERROR_PASSWORD], &room->node);
					return;
				}
			}
			if (room->flags & MUC_FLAG_MEMBERSONLY && affiliation < AFFIL_MEMBER) {
				worker_bounce(ingress, &error_definitions[ERROR_MEMBERS_ONLY], &room->node);
				return;
			}
		}

		sender = room_join(room, &ingress->real_from, &ingress->proxy_to.resource, affiliation);
		packet_cleanup(ingress, PACKET_CLEANUP_FROM | PACKET_CLEANUP_TO);
		
		room_presences_send(room, sender);

		just_joined = TRUE;
	}

	BufferPtr presence_buffer = {};
	if (sender->role > ROLE_VISITOR || room->flags & MUC_FLAG_VISITORPRESENCE) {
		// TODO(artem): optimize this thing
		presence_buffer = sender->presence;
		sender->presence = ingress->inner;
	}
	if (ingress->type == STANZA_PRESENCE_UNAVAILABLE) {
		sender->role = ROLE_NONE;
	}
	room_broadcast_presence(room, sender, 0, 0);

	if (ingress->type == STANZA_PRESENCE_UNAVAILABLE) {
		free(presence_buffer.data);
		BPT_INIT(&sender->presence);
		room_leave(room, sender);
	} else {
		if (just_joined) {
			// TODO(artem): history_send(&room->node, history_first_item(&room->history, &muc_node), sender);
			room_subject_send(room, sender);
		}

		if (BPT_SIZE(&sender->presence) <= REASONABLE_RAW_LIMIT) {
			// Cache participant's presence to broadcast it to newcomers
			if (BPT_BLANK(&sender->presence)) {
				free(presence_buffer.data);
				BPT_INIT(&sender->presence);
			} else {
				presence_buffer.data = realloc(presence_buffer.data, BPT_SIZE(&sender->presence));
				presence_buffer.end = presence_buffer.data + BPT_SIZE(&sender->presence);
				memcpy(presence_buffer.data, sender->presence.data, BPT_SIZE(&sender->presence));
				sender->presence = presence_buffer;
			}
		} else {
			BPT_INIT(&sender->presence);
			free(presence_buffer.data);
		}
	}

	room->flags &= ~MUC_FLAG_JUST_CREATED;
}

static int role_by_name(BufferPtr *name) {
	for (int role = ROLE_NONE; role <= ROLE_MODERATOR; ++role) {
		if (BPT_EQ_BIN(role_names[role], name, role_name_sizes[role])) {
			return role;
		}
	}

	return ROLE_UNCHANGED;
}

static int next_muc_admin_item(BufferPtr *node, MucAdminItem *target) {
	XmlAttr node_attr;
	XmlNodeTraverser nodes = { .buffer = *node };
	int attr_state;

	jid_init(&target->jid);
	target->role = ROLE_UNCHANGED;
	target->affiliation = AFFIL_UNCHANGED;
	BPT_INIT(&target->nick);
	BPT_INIT(&target->reason_node);

	while (xmlfsm_next_sibling(&nodes)) {
		if (!BUF_EQ_LIT("item", &nodes.node_name)) {
			continue;
		}

		while ((attr_state = xmlfsm_next_attr(&nodes.node, &node_attr)) == XMLPARSE_SUCCESS) {
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

		if (attr_state == XMLNODE_NOATTR) {
			xmlfsm_skipto_node(&nodes.node, "reason", &target->reason_node);
		}

		*node = nodes.buffer;
		return TRUE;
	}
	return FALSE;
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
	} else if (BPT_EQ_LIT("muc#roomconfig_allowvisitorpresence", var)) {
		if (btoi(value)) {
			room->flags |= MUC_FLAG_VISITORPRESENCE;
		} else {
			room->flags &= ~MUC_FLAG_VISITORPRESENCE;
		}
	} else if (BPT_EQ_LIT("muc#roomconfig_mewcat", var)) {
		if (btoi(value)) {
			room->flags |= MUC_FLAG_MEWCAT;
		} else {
			room->flags &= ~MUC_FLAG_MEWCAT;
		}
	}
	return TRUE;
}

static BOOL room_config_parse(Room *room, BufferPtr *data, int acl) {
	XmlNodeTraverser field_nodes = { .buffer = *data }, value_nodes;
	XmlAttr var;
	while (xmlfsm_next_sibling(&field_nodes)) {
		if (!BUF_EQ_LIT("field", &field_nodes.node_name) ||
				!xmlfsm_skipto_attr(&field_nodes.node, "var", &var) ||
				!xmlfsm_skip_attrs(&field_nodes.node)) {
			continue;
		}
		value_nodes.buffer = field_nodes.node;
		while (xmlfsm_next_sibling(&value_nodes)) {
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

static void route_iq(Room *room, IncomingPacket *ingress, ACLConfig *acl) {
	BuilderPacket egress = {};
	Participant *sender = 0, *receiver = 0,
				*first_affected_participant = 0,
				*current_affected_participant = 0;
	MucAdminItem target;

	BufferPtr xmlns = {};
	XmlAttr node_attr;
	XmlNodeTraverser nodes = { .buffer = ingress->inner };
	int admin_state, status_codes;
	BOOL is_query_node_empty;

	sender = participants_find_by_jid(&room->participants, &ingress->real_from);
	if (!BPT_NULL(&ingress->proxy_to.resource)) {
		// iq directed to participant - just proxying
		if (!sender) {
			worker_bounce(ingress, &error_definitions[ERROR_EXTERNAL_IQ], &room->node);
			return;
		}
		if (!(room->flags & MUC_FLAG_IQ_PROXY)) {
			worker_bounce(ingress, &error_definitions[ERROR_IQ_PROHIBITED], &room->node);
			return;
		}
		if (!(receiver = participants_find_by_nick(&room->participants, &ingress->proxy_to.resource))) {
			worker_bounce(ingress, &error_definitions[ERROR_RECIPIENT_NOT_IN_ROOM], &room->node);
			return;
		}

		packet_cleanup(ingress, PACKET_CLEANUP_FROM | PACKET_CLEANUP_TO);

		egress.from_nick = sender->nick;
		egress.user_data = ingress->inner;
		egress.to = receiver->jid;

		// special routing for vcard request - 'to' should not contain resource
		if (ingress->type == STANZA_IQ_GET) {
			nodes.buffer = ingress->inner;
			if (xmlfsm_next_sibling(&nodes)) {
				if (BUF_EQ_LIT("vCard", &nodes.node_name)) {
					BPT_INIT(&egress.to.resource);
				}
			}
		}

#ifdef MEWCAT
		if (!mewcat_handle(room, sender, receiver, &egress)) {
			return;
		}
#endif
		worker_send(&egress);
		return;
	}

	egress.type = STANZA_IQ_RESULT;

	// first find <query> node and it's xmlns
	while (xmlfsm_next_sibling(&nodes)) {
		if (BUF_EQ_LIT("query", &nodes.node_name)) {
			if (xmlfsm_skipto_attr(&nodes.node, "xmlns", &node_attr)) {
				xmlns = node_attr.value;
				is_query_node_empty = !xmlfsm_skip_attrs(&nodes.node);
				break;
			}
		}
	}

	if (BPT_NULL(&xmlns)) {
		worker_bounce(ingress, &error_definitions[ERROR_IQ_BAD], &room->node);
		return;
	}

	LDEBUG("received iq:%c, xmlns '%.*s'", ingress->type, BPT_SIZE(&xmlns), xmlns.data);
	if (ingress->type == STANZA_IQ_GET) {
		if (BPT_EQ_LIT("http://jabber.org/protocol/disco#info", &xmlns)) {
			egress.iq_type = BUILD_IQ_ROOM_DISCO_INFO;
			egress.sys_data.room = room;
		} else if (BPT_EQ_LIT("http://jabber.org/protocol/disco#items", &xmlns)) {
			if (room->flags & MUC_FLAG_PUBLICPARTICIPANTS) {
				egress.iq_type = BUILD_IQ_ROOM_DISCO_ITEMS;
				egress.sys_data.room = room;
			} else {
				worker_bounce(ingress, &error_definitions[ERROR_NOT_IMPLEMENTED], &room->node);
				return;
			}
		} else if (BPT_EQ_LIT("http://jabber.org/protocol/muc#admin", &xmlns)) {
			if (is_query_node_empty) {
				worker_bounce(ingress, &error_definitions[ERROR_IQ_BAD], &room->node);
				return;
			}

			if (!sender) {
				worker_bounce(ingress, &error_definitions[ERROR_EXTERNAL_IQ], &room->node);
				return;
			}
			if (sender->affiliation < AFFIL_ADMIN) {
				worker_bounce(ingress, &error_definitions[ERROR_PRIVILEGE_LEVEL], &room->node);
				return;
			}

			if (next_muc_admin_item(&nodes.node, &target) <= 0 ||
					target.affiliation == AFFIL_UNCHANGED ||
					target.affiliation == AFFIL_NONE) {
				worker_bounce(ingress, &error_definitions[ERROR_IQ_BAD], &room->node);
				return;
			}

			egress.iq_type = BUILD_IQ_ROOM_AFFILIATIONS;
			egress.sys_data.muc_items.affiliation = target.affiliation;
			egress.sys_data.muc_items.items = room->affiliations[egress.sys_data.muc_items.affiliation].head;
		} else if (BPT_EQ_LIT("http://jabber.org/protocol/muc#owner", &xmlns)) {
			if  (!sender) {
				worker_bounce(ingress, &error_definitions[ERROR_EXTERNAL_IQ], &room->node);
				return;
			}
			if (sender->affiliation < AFFIL_OWNER) {
				worker_bounce(ingress, &error_definitions[ERROR_PRIVILEGE_LEVEL], &room->node);
				return;
			}

			egress.iq_type = BUILD_IQ_ROOM_CONFIG;
			egress.sys_data.room = room;
		}
	} else if (ingress->type == STANZA_IQ_SET) {
		if (BPT_EQ_LIT("http://jabber.org/protocol/muc#admin", &xmlns)) {
			if (is_query_node_empty) {
				worker_bounce(ingress, &error_definitions[ERROR_IQ_BAD], &room->node);
				return;
			}

			if (!sender) {
				worker_bounce(ingress, &error_definitions[ERROR_EXTERNAL_IQ], &room->node);
				return;
			}

			if (sender->role < ROLE_MODERATOR) {
				worker_bounce(ingress, &error_definitions[ERROR_PRIVILEGE_LEVEL], &room->node);
				return;
			}

			while ((admin_state = next_muc_admin_item(&nodes.node, &target))) {
				if (admin_state < 0) {
					worker_bounce(ingress, &error_definitions[ERROR_IQ_BAD], &room->node);
					return;
				}
				if ((target.role != ROLE_UNCHANGED && BPT_BLANK(&target.nick)) ||
						(target.affiliation != AFFIL_UNCHANGED && JID_EMPTY(&target.jid)) ||
						((target.role == ROLE_UNCHANGED) == (target.affiliation == AFFIL_UNCHANGED))) {
					worker_bounce(ingress, &error_definitions[ERROR_IQ_BAD], &room->node);
					return;
				}

				if (target.role != ROLE_UNCHANGED) {
					receiver = participants_find_by_nick(&room->participants, &target.nick);
					if (!receiver) {
						worker_bounce(ingress, &error_definitions[ERROR_RECIPIENT_NOT_IN_ROOM], &room->node);
						return;
					}
					if (receiver->affiliation >= AFFIL_ADMIN || // no one can change role of admin+
							(sender->affiliation < AFFIL_ADMIN && // if just a moderator,
							 (receiver->role >= ROLE_MODERATOR || // may only change roles of non-moderators
							  target.role >= ROLE_MODERATOR))) { // to non-moderators
						worker_bounce(ingress, &error_definitions[ERROR_PRIVILEGE_LEVEL], &room->node);
						return;
					}
					receiver->role = target.role;

					participant_set_affected(&first_affected_participant,
							&current_affected_participant, receiver, &target.reason_node);
				}

				if (target.affiliation != AFFIL_UNCHANGED) {
					if (sender->affiliation < AFFIL_ADMIN) {
						worker_bounce(ingress, &error_definitions[ERROR_PRIVILEGE_LEVEL], &room->node);
						return;
					}

					if (!affiliationss_add(room->affiliations, sender, target.affiliation,
								&target.jid, &target.reason_node)) {
						worker_bounce(ingress, &error_definitions[ERROR_PRIVILEGE_LEVEL], &room->node);
						return;
					}

					DLS_FOREACH(&room->participants, receiver) {
						target.affiliation = affiliationss_get_by_jid((AffiliationsList **)room->affiliations, acl, &receiver->jid);
						if (target.affiliation != receiver->affiliation) {
							receiver->role =
								(target.affiliation >= AFFIL_ADMIN) ? ROLE_MODERATOR :
								(target.affiliation == AFFIL_OUTCAST) ? ROLE_NONE :
								ROLE_PARTICIPANT;
							receiver->affiliation = target.affiliation;
							participant_set_affected(&first_affected_participant,
									&current_affected_participant, receiver, &target.reason_node);
						}
					}
				}
			}

			egress.iq_type = BUILD_IQ_ROOM_AFFILIATIONS;
			egress.sys_data.muc_items.items = 0;
		} else if (BPT_EQ_LIT("http://jabber.org/protocol/muc#owner", &xmlns)) {
			if (is_query_node_empty) {
				worker_bounce(ingress, &error_definitions[ERROR_IQ_BAD], &room->node);
				return;
			}

			if (!sender) {
				worker_bounce(ingress, &error_definitions[ERROR_EXTERNAL_IQ], &room->node);
				return;
			}
			if (sender->affiliation < AFFIL_OWNER) {
				worker_bounce(ingress, &error_definitions[ERROR_PRIVILEGE_LEVEL], &room->node);
				return;
			}

			nodes.buffer = nodes.node;
			while (xmlfsm_next_sibling(&nodes)) {
				if (BUF_EQ_LIT("destroy", &nodes.node_name)) {
					room->flags |= MUC_FLAG_DESTROYED;
					nodes.node.data = nodes.node_start;
					DLS_FOREACH(&room->participants, receiver) {
						receiver->role = ROLE_NONE;
						receiver->affiliation = AFFIL_NONE;
						egress.sys_data.presence.resume = nodes.node;
						participant_set_affected(&first_affected_participant,
								&current_affected_participant, receiver, 0);
					}
					egress.iq_type = BUILD_IQ_EMPTY;
					break;
				} else if (BUF_EQ_LIT("x", &nodes.node_name)) {
					if (xmlfsm_skip_attrs(&nodes.node)) {
						if (!room_config_parse(room, &nodes.node, acl_role(acl, &sender->jid))) {
							worker_bounce(ingress, &error_definitions[ERROR_IQ_BAD], &room->node);
							return;
						}
					}
					if (room->flags & MUC_FLAG_MEMBERSONLY) {
						BPT_SET_LIT(&nodes.node, "<reason>Room policy switched to members-only</reason>");
						DLS_FOREACH(&room->participants, receiver) {
							if (receiver->affiliation < AFFIL_MEMBER) {
								receiver->role = ROLE_NONE;
								participant_set_affected(&first_affected_participant,
										&current_affected_participant, receiver, &nodes.node);
							}
						}
					}
					egress.iq_type = BUILD_IQ_EMPTY;
					break;
				}
			}
		}
	}

	if (egress.iq_type) {
		packet_cleanup(ingress, PACKET_CLEANUP_SWITCH_FROM_TO);
		worker_send(&egress);
	} else {
		worker_bounce(ingress, &error_definitions[ERROR_NOT_IMPLEMENTED], &room->node);
		return;
	}

	for (current_affected_participant = first_affected_participant;
			current_affected_participant; ) {
		current_affected_participant->affected_list.included = FALSE;
		status_codes = 0;
		if (room->flags & MUC_FLAG_DESTROYED) {
			current_affected_participant->role = ROLE_NONE;
		} else if (current_affected_participant->affiliation == AFFIL_OUTCAST) {
			current_affected_participant->role = ROLE_NONE;
			status_codes |= STATUS_BANNED;
		} else if (room->flags & MUC_FLAG_MEMBERSONLY &&
				current_affected_participant->affiliation < AFFIL_MEMBER) {
			current_affected_participant->role = ROLE_NONE;
			status_codes |= STATUS_NONMEMBER_REMOVED;
		} else if (current_affected_participant->role == ROLE_NONE) {
			current_affected_participant->role = ROLE_NONE;
			status_codes |= STATUS_KICKED;
		}

		if (current_affected_participant->role == ROLE_NONE) {
			free(current_affected_participant->presence.data);
			BPT_INIT(&current_affected_participant->presence);
		}

		room_broadcast_presence(room, current_affected_participant, 0, status_codes);
		BPT_INIT(&current_affected_participant->affected_list.reason_node);

		if (current_affected_participant->role == ROLE_NONE) {
			first_affected_participant = current_affected_participant->affected_list.next;
			room_leave(room, current_affected_participant);
			current_affected_participant = first_affected_participant;
		} else {
			current_affected_participant = current_affected_participant->affected_list.next;
		}
	}
}

void room_route(Room *room, IncomingPacket *ingress, ACLConfig *acl, int deciseconds_limit) {
	Participant *sender;
	if (ingress->type == STANZA_ERROR && ingress->name != STANZA_IQ) {
		// <message> or <presence> with type='error'
		if ((sender = participants_find_by_jid(&room->participants, &ingress->real_from))) {
			free(sender->presence.data);
			BPT_SET_LIT(&sender->presence, "<status>This occupant is kicked from the room because he sent an error stanza</status>");
			sender->role = ROLE_NONE;
			packet_cleanup(ingress, PACKET_CLEANUP_FROM | PACKET_CLEANUP_TO);
			room_broadcast_presence(room, sender, 0, 0);
			BPT_INIT(&sender->presence);
			room_leave(room, sender);
		}
		return;
	}

	switch (ingress->name) {
		case STANZA_MESSAGE:
			route_message(room, ingress, deciseconds_limit);
			break;
		case STANZA_PRESENCE:
			route_presence(room, ingress, acl);
			break;
		case STANZA_IQ:
			route_iq(room, ingress, acl);
			break;
	}
}
