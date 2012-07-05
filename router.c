#include <string.h>

#include "xmcomp/common.h"
#include "room.h"

#include "router.h"

#define SEND(callback, send_data) \
	((callback)->proc((callback)->data, (send_data)))

void cleanup_erase(IncomingPacket *packet) {
	int index;
	for (index = 0; index < MAX_ERASE_CHUNKS && packet->erase[index].data; ++index) {
		memset(packet->erase[index].data, ' ', BPT_SIZE(&packet->erase[index]));
	}
}

int cutoff_mucnode(IncomingPacket *packet) {
	BufferPtr buffer = packet->inner, node = packet->inner;
	Buffer node_name;
	XmlAttr attr;
	int erase_index;
	char *node_start = 0;

	for (erase_index = 0; erase_index < MAX_ERASE_CHUNKS; ++erase_index) {
		if (!packet->erase[erase_index].data) {
			break;
		}
	}

	for (; xmlfsm_skip_node(&buffer, 0, 0) == XMLPARSE_SUCCESS; node.data = buffer.data) {
		node.end = buffer.data;
		node_start = node.data;

		LDEBUG("seeing if '%.*s' is a muc#user node",
				BPT_SIZE(&node), node.data);

		xmlfsm_node_name(&node, &node_name);
		if (node_name.size == 1 && *node_name.data == 'x') {
			while (xmlfsm_get_attr(&node, &attr) == XMLPARSE_SUCCESS) {
				if (
						!BPT_EQ_LIT("xmlns", &attr.name) &&
						!BPT_EQ_LIT("http://jabber.org/protocol/muc#user", &attr.value)) {
					LDEBUG("found a muc#user node, setting erase");
					if (erase_index == MAX_ERASE_CHUNKS) {
						LWARN("cannot allocate an erase chunk; muc#user is not removable, thus dropping stanza");
						return 0;
					}
					packet->erase[erase_index].data = node_start;
					packet->erase[erase_index].end = node.end;
					++erase_index;
				}
			}
		}
	}

	return 1;
}

int send_to(BuilderPacket *output, SendCallback *send, ParticipantEntry *receiver, int limit) {
	int sent = 0;
	for (; receiver && sent < limit; receiver = receiver->next, ++sent) {
		LDEBUG("routing stanza to '%.*s', real JID '%.*s'",
				receiver->nick.size, receiver->nick.data,
				JID_LEN(&receiver->jid), JID_STR(&receiver->jid));
		output->to = receiver->jid;
		if (!SEND(send, output)) {
			++sent;
		}
	}

	return sent;
}

int route(IncomingPacket *packet, SendCallback *send, char *hostname) {
	ParticipantEntry *sender = 0, *receiver = 0;
	BuilderPacket output;
	Room *room = packet->room;
	int routed_receivers = 0;

	LDEBUG("routing packet:\n"
			" ** Room: %.*s@<this vhost>\n"
			" ** Stanza '%c' type '%c'\n"
			" ** '%.*s' -> '%.*s'\n"
			" ** Header: '%.*s'\n"
			" ** Data: '%.*s'",
			room->node.size, room->node.data,
			packet->name, packet->type,
			JID_LEN(&packet->real_from), JID_STR(&packet->real_from),
			JID_LEN(&packet->proxy_to), JID_STR(&packet->proxy_to),
			BPT_SIZE(&packet->header), packet->header.data,
			BPT_SIZE(&packet->inner), packet->inner.data);

	memset(&output, 0, sizeof(output));
	output.name = packet->name;
	output.type = packet->type;
	output.from_node = room->node;
	output.from_host.data = hostname; // XXX(artem): optimization?
	output.from_host.size = strlen(hostname);
	output.header = packet->header;

	sender = room_participant_by_jid(room, &packet->real_from);
	if (sender) {
		output.from_nick = sender->nick;
		LDEBUG("got sender JID='%.*s', nick='%.*s'",
				JID_LEN(&sender->jid), JID_STR(&sender->jid),
				sender->nick.size, sender->nick.data);
	} else {
		LDEBUG("sender is not in the room yet");
	}
	
	switch (packet->name) {
		case 'm':
			output.user_data = packet->inner;
			if (sender) {
				if (packet->type == 'c') {
					if ((receiver = room_participant_by_nick(room, &packet->proxy_to.resource))) {
						LDEBUG("sending private message to '%.*s', real JID '%.*s'",
								receiver->nick.size, receiver->nick.data,
								JID_LEN(&receiver->jid), JID_STR(&receiver->jid));
						cleanup_erase(packet);
						routed_receivers += send_to(&output, send, receiver, 1);
					}
				} else {
					cleanup_erase(packet);
					routed_receivers += send_to(&output, send, room->participants, 1 << 30);
				}
			}
			break;
		case 'p':
			if (!cutoff_mucnode(packet)) {
				return 0;
			}
			output.user_data = packet->inner;
			if (!sender && packet->type != 'u') {
				receiver = room_join(room, &packet->real_from, &packet->proxy_to.resource);
				cleanup_erase(packet);

				LDEBUG("joined the room as '%.*s', real JID '%.*s'",
						receiver->nick.size, receiver->nick.data,
						JID_LEN(&receiver->jid), JID_STR(&receiver->jid));
				output.to = receiver->jid;

				// Skip first participant as we know this is the one who just joined
				for (sender = room->participants->next; sender; sender = sender->next) {
					output.from_nick = sender->nick;
					output.participant.affiliation = sender->affiliation;
					output.participant.role = sender->role;
					if (receiver->role == ROLE_MODERATOR || room->flags & MUC_FLAG_SEMIANONYMOUS) {
						output.participant.jid = &sender->jid;
					}
					routed_receivers += send_to(&output, send, receiver, 1);
				}

				sender = receiver;
				output.from_nick = sender->nick;
			} else {
				cleanup_erase(packet);
			}

			if (sender) {
				output.participant.affiliation = sender->affiliation;
				output.participant.role = sender->role;
				for (receiver = room->participants; receiver; receiver = receiver->next) {
					output.to = receiver->jid;
					if (receiver->role == ROLE_MODERATOR || room->flags & MUC_FLAG_SEMIANONYMOUS) {
						output.participant.jid = &sender->jid;
					}
					routed_receivers += send_to(&output, send, receiver, 1);
				}

				if (packet->type == 'u') {
					LDEBUG("leaving the room");
					room_leave(room, sender);
				}
			}
			break;
	}

	return routed_receivers;
}
