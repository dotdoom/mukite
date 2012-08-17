#include <stdlib.h>
#include <string.h>

#include "xmcomp/logger.h"
#include "router.h"

#include "room.h"

#define STATUS_NON_ANONYMOUS 100
#define STATUS_SELF_PRESENCE 110
#define STATUS_LOGGING_ENABLED 170
#define STATUS_NICKNAME_ENFORCED 210
#define STATUS_BANNED 301
#define STATUS_NICKNAME_CHANGED 303
#define STATUS_KICKED 307
#define STATUS_NONMEMBER_REMOVED 321

static XMPPError error_definitions[] = {
	{
#define ERROR_EXTERNAL_MESSAGE 0
		.code = "405",
		.name = "not-allowed",
		.type = "cancel",
		.text = "Only participants are allowed to send messages to this conference"
	}, {
#define ERROR_PARTICIPANT_NOT_IN_ROOM 1
		.code = "404",
		.name = "item-not-found",
		.type = "cancel",
		.text = "Participant is not in the conference room"
	}, {
#define ERROR_NO_VISITORS_PM 2
		.code = "403",
		.name = "forbidden",
		.type = "cancel",
		.text = "Visitors are not allowed to send private messages here"
	}, {
#define ERROR_NO_VISITORS_PUBLIC 3
		.code = "403",
		.name = "forbidden",
		.type = "cancel",
		.text = "Visitors are not allowed to send messages to the public chat"
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
	}
};

void room_init(Room *room, BufferPtr *node) {
	memset(room, 0, sizeof(*room));
	pthread_mutex_init(&room->sync, 0);
	room->node.size = BPT_SIZE(node);
	room->node.data = malloc(room->node.size);
	memcpy(room->node.data, node->data, room->node.size);
	room->flags = MUC_FLAGS_DEFAULT;
	room->max_participants = 100;

	room->default_role = ROLE_PARTICIPANT;
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

inline AffiliationEntry *find_affiliation(AffiliationEntry *entry, Jid *jid) {
	for (; entry; entry = entry->next) {
		if (!jid_cmp(&entry->jid, jid, JID_FULL | JID_CMP_NULLWC)) {
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


ParticipantEntry *room_join(Room *room, Jid *jid, BufferPtr *nick, int affiliation) {
	ParticipantEntry *participant = malloc(sizeof(*participant));

	LDEBUG("'%.*s' is joining the room as '%.*s'",
			JID_LEN(jid), JID_STR(jid),
			BPT_SIZE(nick), nick->data);

	jid_cpy(&participant->jid, jid, JID_FULL);

	participant->nick.size = BPT_SIZE(nick);
	participant->nick.data = malloc(participant->nick.size);
	memcpy(participant->nick.data, nick->data, participant->nick.size);

	if (room->participants) {
		room->participants->prev = participant;
	}
	participant->next = room->participants;
	participant->prev = 0;
	room->participants = participant;

	++room->participants_count;

	participant->affiliation = affiliation;
	participant->role = room_role_for_affiliation(room, affiliation);

	LDEBUG("created new participant #%d, role %d affil %d",
			room->participants_count, participant->role, participant->affiliation);

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

	LDEBUG("participant #%d '%.*s' (JID '%.*s') has left '%.*s'",
			room->participants_count, participant->nick.size, participant->nick.data,
			JID_LEN(&participant->jid), JID_STR(&participant->jid),
			room->node.size, room->node.data);
	--room->participants_count;

	jid_destroy(&participant->jid);
	if (participant->presence.data) {
		free(participant->presence.data);
	}
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

BOOL participants_serialize(ParticipantEntry *list, FILE *output) {
	for (; list; list = list->next) {
		if (!jid_serialize(&list->jid, output) ||
			!buffer_serialize(&list->nick, output) ||
			!fwrite(&list->affiliation, sizeof(list->affiliation), 1, output) ||
			!fwrite(&list->role, sizeof(list->role), 1, output) ||
			!fwrite(&list->next, sizeof(list->next), 1, output)) {
			return FALSE;
		}
	}
	return TRUE;
}

BOOL participants_deserialize(ParticipantEntry **list, FILE *output, int limit) {
	return TRUE;
}

BOOL affiliations_serialize(AffiliationEntry *list, FILE *output) {
	for (; list; list = list->next) {
		if (!jid_serialize(&list->jid, output) ||
			!buffer_serialize(&list->reason, output) ||
			!fwrite(&list->next, sizeof(list->next), 1, output)) {
			return FALSE;
		}
	}
	return TRUE;
}

BOOL affiliations_deserialize(AffiliationEntry **list, FILE *input, int limit) {
	return TRUE;
}

BOOL room_serialize(Room *room, FILE *output) {
	return
		buffer_serialize(&room->node, output) &&
		buffer_serialize(&room->title, output) &&
		buffer_serialize(&room->description, output) &&
		buffer_serialize(&room->subject, output) &&
		buffer_serialize(&room->password, output) &&

		fwrite(&room->flags, sizeof(room->flags), 1, output) &&
		fwrite(&room->default_role, sizeof(room->default_role), 1, output) &&
		fwrite(&room->max_participants, sizeof(room->max_participants), 1, output) &&
		fwrite(&room->_unused, sizeof(room->_unused), 1, output) &&

		participants_serialize(room->participants, output) &&
		affiliations_serialize(room->owners, output) &&
		affiliations_serialize(room->admins, output) &&
		affiliations_serialize(room->members, output) &&
		affiliations_serialize(room->outcasts, output);
}

BOOL room_deserialize(Room *room, FILE *input) {
	return
		buffer_deserialize(&room->node, input, MAX_JID_PART_SIZE) &&
		buffer_deserialize(&room->title, input, USER_STRING_OPTION_LIMIT) &&
		buffer_deserialize(&room->description, input, USER_STRING_OPTION_LIMIT) &&
		buffer_deserialize(&room->subject, input, USER_STRING_OPTION_LIMIT) &&
		buffer_deserialize(&room->password, input, USER_STRING_OPTION_LIMIT) &&

		fread(&room->flags, sizeof(room->flags), 1, input) &&
		fread(&room->default_role, sizeof(room->default_role), 1, input) &&
		fread(&room->max_participants, sizeof(room->max_participants), 1, input) &&
		fread(&room->_unused, sizeof(room->_unused), 1, input) &&

		participants_deserialize(&room->participants, input, PARTICIPANTS_LIMIT) &&
		affiliations_deserialize(&room->owners, input, AFFILIATION_LIST_LIMIT) &&
		affiliations_deserialize(&room->admins, input, AFFILIATION_LIST_LIMIT) &&
		affiliations_deserialize(&room->members, input, AFFILIATION_LIST_LIMIT) &&
		affiliations_deserialize(&room->outcasts, input, AFFILIATION_LIST_LIMIT);
}

BOOL erase_muc_user_node(IncomingPacket *packet) {
	BufferPtr buffer = packet->inner, node = buffer;
	Buffer node_name;
	XmlAttr attr;
	int erase_index = 0;
	char *node_start = 0;

	// First find the next free index of 'erase' block
	for (; erase_index < MAX_ERASE_CHUNKS; ++erase_index) {
		if (!packet->erase[erase_index].data) {
			break;
		}
	}

	for (; xmlfsm_skip_node(&buffer, 0, 0) == XMLPARSE_SUCCESS; node.data = buffer.data) {
		node.end = buffer.data;
		node_start = node.data;

		xmlfsm_node_name(&node, &node_name);
		if (node_name.size == 1 && *node_name.data == 'x') {
			while (xmlfsm_get_attr(&node, &attr) == XMLPARSE_SUCCESS) {
				if (
						BPT_EQ_LIT("xmlns", &attr.name) &&
						BPT_EQ_LIT("http://jabber.org/protocol/muc#user", &attr.value)) {
					LDEBUG("found a muc#user node, setting erase");
					if (erase_index == MAX_ERASE_CHUNKS) {
						LWARN("cannot allocate an erase chunk; muc#user is not removable, thus dropping stanza");
						return FALSE;
					}
					packet->erase[erase_index].data = node_start;
					packet->erase[erase_index].end = node.end;
					++erase_index;
				}
			}
		}
	}

	return TRUE;
}

void send_to_participants(BuilderPacket *output, SendCallback *send, ParticipantEntry *participant, int limit) {
	int sent = 0;
	for (; participant && sent < limit; participant = participant->next, ++sent) {
		LDEBUG("routing stanza to '%.*s', real JID '%.*s'",
				participant->nick.size, participant->nick.data,
				JID_LEN(&participant->jid), JID_STR(&participant->jid));
		output->to = participant->jid;
		send->proc(send->data);
	}
}

BOOL room_attach_config_status_codes(Room *room, MucAdmNode *participant) {
	if ((room->flags & MUC_FLAG_SEMIANONYMOUS) != MUC_FLAG_SEMIANONYMOUS) {
		if (!builder_push_status_code(participant, STATUS_NON_ANONYMOUS)) {
			return FALSE;
		}
	}

	if ((room->flags & MUC_FLAG_ENABLELOGGING) == MUC_FLAG_ENABLELOGGING) {
		if (!builder_push_status_code(participant, STATUS_LOGGING_ENABLED)) {
			return FALSE;
		}
	}

	return TRUE;
}

BOOL room_broadcast_presence(Room *room, BuilderPacket *output, SendCallback *send, ParticipantEntry *sender) {
	int orig_status_codes_count = output->participant.status_codes_count;
	ParticipantEntry *receiver = room->participants;

	for (; receiver; receiver = receiver->next) {
		if (receiver == sender) {
			if (!room_attach_config_status_codes(room, &output->participant)) {
				LERROR("[BUG] unexpected status codes buffer overflow when trying to push room config");
				return FALSE;
			}
			builder_push_status_code(&output->participant, STATUS_SELF_PRESENCE);
		} else {
			output->participant.status_codes_count = orig_status_codes_count;
		}
		output->to = receiver->jid;
		if (receiver->role == ROLE_MODERATOR ||
				(room->flags & MUC_FLAG_SEMIANONYMOUS) != MUC_FLAG_SEMIANONYMOUS) {
			output->participant.jid = &sender->jid;
		} else {
			output->participant.jid = 0;
		}
		send->proc(send->data);
	}
	return TRUE;
}

void room_route(Room *room, RouterChunk *chunk) {
	ParticipantEntry *sender = 0, *receiver = 0;
	IncomingPacket *input = &chunk->input;
	BuilderPacket *output = &chunk->output;
	int affiliation;
	AffiliationEntry *affiliation_entry;
	BufferPtr new_nick;

	output->from_node = room->node;

	sender = room_participant_by_jid(room, &input->real_from);
	if (sender) {
		output->from_nick = sender->nick;
		LDEBUG("got sender in the room, JID='%.*s', nick='%.*s'",
				JID_LEN(&sender->jid), JID_STR(&sender->jid),
				sender->nick.size, sender->nick.data);
	} else {
		LDEBUG("sender is not in the room yet");
	}
	
	switch (input->name) {
		case 'm':
			if (!sender) {
				router_error(chunk, &error_definitions[ERROR_EXTERNAL_MESSAGE]);
				return;
			}

			// TODO(artem): check words filter for input->inner

			output->user_data = input->inner;
			if (input->type == 'c') {
				if (!(receiver = room_participant_by_nick(room, &input->proxy_to.resource))) {
					router_error(chunk, &error_definitions[ERROR_PARTICIPANT_NOT_IN_ROOM]);
					return;
				}

				if (sender->role == ROLE_VISITOR && (room->flags & MUC_FLAG_VISITORSPM) == MUC_FLAG_VISITORSPM) {
					router_error(chunk, &error_definitions[ERROR_NO_VISITORS_PM]);
					return;
				}

				LDEBUG("sending private message to '%.*s', real JID '%.*s'",
						receiver->nick.size, receiver->nick.data,
						JID_LEN(&receiver->jid), JID_STR(&receiver->jid));
				router_cleanup(input);
				send_to_participants(output, &chunk->send, receiver, 1);
			} else {
				if (sender->role < ROLE_PARTICIPANT) {
					router_error(chunk, &error_definitions[ERROR_NO_VISITORS_PUBLIC]);
					return;
				}

				router_cleanup(input);
				send_to_participants(output, &chunk->send, room->participants, 1 << 30);
			}
			break;
		case 'p':
			if (!erase_muc_user_node(input)) {
				return; // stanza is obviously not valid, just drop silently
			}

			if (!sender) {
				// Participant not in room, but wants to join
				if (input->type == 'u') {
					// There's no use in 'unavailable' presence for the not-in-room user
					return;
				}

				if (room_participant_by_nick(room, &input->proxy_to.resource)) {
					router_error(chunk, &error_definitions[ERROR_OCCUPANT_CONFLICT]);
					return;
				}

				// TODO(artem): check global registered nickname
				// TODO(artem): check password
				// TODO(artem): load room history as requested

				if ((acl_role(chunk->acl, &input->real_from) & ACL_MUC_ADMIN) == ACL_MUC_ADMIN) {
					affiliation = AFFIL_OWNER;
				} else {
					if (room->participants_count >= room->max_participants) {
						router_error(chunk, &error_definitions[ERROR_OCCUPANTS_LIMIT]);
						return;
					}
					if (!room->participants && (room->flags & MUC_FLAG_PERSISTENTROOM) != MUC_FLAG_PERSISTENTROOM) {
						// This is empty non-persistent room => thus it was just created
						affiliation_entry = malloc(sizeof(*affiliation_entry));
						memset(affiliation_entry, 0, sizeof(*affiliation_entry));
						jid_cpy(&affiliation_entry->jid, &input->real_from, JID_NODE | JID_HOST);
						room->owners = affiliation_entry;
					}
					affiliation = room_get_affiliation(room, &input->real_from);
					if (affiliation < AFFIL_NONE) {
						// TODO(artem): show the reason of being banned?
						router_error(chunk, &error_definitions[ERROR_BANNED]);
						return;
					}
					if ((room->flags & MUC_FLAG_MEMBERSONLY) == MUC_FLAG_MEMBERSONLY &&
							affiliation < AFFIL_MEMBER) {
						router_error(chunk, &error_definitions[ERROR_MEMBERS_ONLY]);
						return;
					}
				}

				receiver = room_join(room, &input->real_from, &input->proxy_to.resource, affiliation);
				router_cleanup(input);

				// Skip first participant as we know this is the one who just joined
				for (sender = room->participants->next; sender; sender = sender->next) {
					output->from_nick = sender->nick;
					output->participant.affiliation = sender->affiliation;
					output->participant.role = sender->role;
					output->user_data = sender->presence;
					if (receiver->role == ROLE_MODERATOR ||
							(room->flags & MUC_FLAG_SEMIANONYMOUS) != MUC_FLAG_SEMIANONYMOUS) {
						output->participant.jid = &sender->jid;
					} else {
						output->participant.jid = 0;
					}
					send_to_participants(output, &chunk->send, receiver, 1);
				}
				sender = receiver;
			} else {
				if ((receiver = room_participant_by_nick(room, &input->proxy_to.resource)) != sender) {
					if (receiver) {
						router_error(chunk, &error_definitions[ERROR_OCCUPANT_CONFLICT]);
						return;
					}

					if (input->type == 'u') {
						// TODO(artem): check how ejabberd handles this
						return;
					}

					// TODO(artem): check globally registered nickname
					// TODO(artem): optimization, when moving to a separate proc

					output->from_nick = sender->nick;
					output->participant.affiliation = sender->affiliation;
					output->participant.role = sender->role;
					new_nick.data = malloc(BPT_SIZE(&input->proxy_to.resource));
					memcpy(new_nick.data, input->proxy_to.resource.data, BPT_SIZE(&input->proxy_to.resource));
					new_nick.end = new_nick.data + BPT_SIZE(&input->proxy_to.resource);
					output->participant.nick = new_nick;
					output->user_data = input->inner;
					output->type = 'u';
					builder_push_status_code(&output->participant, STATUS_NICKNAME_CHANGED);
					router_cleanup(input);

					room_broadcast_presence(room, output, &chunk->send, sender);

					free(sender->nick.data);
					sender->nick.data = new_nick.data;
					sender->nick.size = BPT_SIZE(&new_nick);
					output->type = 0;
					output->participant.nick.data =
						output->participant.nick.end = 0;
				} else {
					router_cleanup(input);
				}

				// TODO(artem): check if visitors can change status
			}

			// Broadcast sender's presence to all participants
			output->participant.affiliation = sender->affiliation;
			output->participant.role = sender->role;
			output->from_nick = sender->nick;
			output->user_data = input->inner;
			room_broadcast_presence(room, output, &chunk->send, sender);

			if (input->type == 'u') {
				room_leave(room, sender);
			} else {
				// Cache participant's presence to broadcast it to newcomers
				if (sender->presence.data) {
					sender->presence.data = realloc(sender->presence.data, BPT_SIZE(&input->inner));
				} else {
					sender->presence.data = malloc(BPT_SIZE(&input->inner));
				}
				sender->presence.end = sender->presence.data + BPT_SIZE(&input->inner);
				memcpy(sender->presence.data, input->inner.data, BPT_SIZE(&input->inner));
			}

			break;
	}
}
