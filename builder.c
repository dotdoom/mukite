#include <string.h>

#include "xmcomp/logger.h"

#include "jid.h"
#include "room.h"

#include "builder.h"

#define BUF_PUSH(data, size) \
	{ \
		chunk_size = (size); \
		if (buffer->data_end + chunk_size > buffer->end) { \
			return FALSE; \
		} \
		memcpy(buffer->data_end, (data), chunk_size); \
		buffer->data_end += chunk_size; \
	}

#define BUF_PUSH_BUF(bptr) \
	BUF_PUSH((bptr).data, (bptr).size)

#define BUF_PUSH_STR(str) \
	BUF_PUSH((str), strlen(str))

#define BUF_PUSH_BPT(bptr) \
	BUF_PUSH((bptr).data, BPT_SIZE(&(bptr)))

#define BUF_PUSH_IFBPT(bptr) \
	{ if (!BPT_NULL(&bptr)) { BUF_PUSH_BPT(bptr) } }

#define BUF_PUSH_LITERAL(data) \
	BUF_PUSH(data, sizeof(data)-1)

#define BUF_PUSH_FMT(format, value) \
	{ \
		chunk_size = snprintf(buffer->data_end, buffer->end - buffer->data_end, format, value); \
		if (chunk_size < 0 || buffer->data_end + chunk_size > buffer->end) { \
			return FALSE; \
		} \
		buffer->data_end += chunk_size; \
	}

BOOL build_presence_mucadm(MucAdmNode *node, BuilderBuffer *buffer) {
	int i, code, chunk_size;

	BUF_PUSH_LITERAL("<x xmlns='http://jabber.org/protocol/muc#user'><item affiliation='");
	BUF_PUSH(affiliation_names[node->affiliation], affiliation_name_sizes[node->affiliation]);
	BUF_PUSH_LITERAL("' role='");
	BUF_PUSH(role_names[node->role], role_name_sizes[node->role]);
	if (node->jid) {
		BUF_PUSH_LITERAL("' jid='");
		BUF_PUSH(JID_STR(node->jid), JID_LEN(node->jid));
	}
	if (!BPT_NULL(&node->nick)) {
		BUF_PUSH_LITERAL("' nick='");
		BUF_PUSH_BPT(node->nick);
	}

	for (i = 0; i < node->status_codes_count && (code = node->status_codes[i]); ++i) {
		BUF_PUSH_LITERAL("'/><status code='");
		BUF_PUSH_FMT("%d", code);
	}

	BUF_PUSH_LITERAL("'/></x>");
	return TRUE;
}

#define BUF_PUSH_STAT(category, value) \
	BUF_PUSH_LITERAL("<stat name='" #category "/" #value "' unit='times' value='"); \
	BUF_PUSH_FMT("%d", data->iq_stats.category->value); \
	BUF_PUSH_LITERAL("'/>");

BOOL build_stats(BuilderBuffer *buffer, BuilderPacket *data) {
	int chunk_size;

	BUF_PUSH_STAT(rooms, count);
	BUF_PUSH_STAT(queue, overflows);
	BUF_PUSH_STAT(queue, underflows);
	BUF_PUSH_STAT(queue, realloc_enlarges);
	BUF_PUSH_STAT(queue, realloc_shortens);
	BUF_PUSH_STAT(queue, mallocs);
	BUF_PUSH_STAT(queue, data_pushes);
	BUF_PUSH_STAT(queue, data_pops);
	BUF_PUSH_STAT(queue, free_pushes);
	BUF_PUSH_STAT(queue, free_pops);
	BUF_PUSH_STAT(ringbuffer, underflows);
	BUF_PUSH_STAT(ringbuffer, overflows);
	BUF_PUSH_STAT(ringbuffer, reads);

	/*

<stat name='time/uptime'/>
<stat name='muc/users'/>
<stat name='muc/jids'/>


<stat name='time/uptime' unit='seconds' value='1024'/>
	 */

	return TRUE;
}

BOOL build_room_items(BuilderBuffer *buffer, Room *room, Buffer *host) {
	int chunk_size;
	ParticipantEntry *participant = room->participants;

	LDEBUG("building room items");
	for (; participant; participant = participant->next) {
		BUF_PUSH_LITERAL("<item name='");
		BUF_PUSH_BUF(participant->nick);
		BUF_PUSH_LITERAL("' jid='");
		BUF_PUSH_BUF(room->node);
		BUF_PUSH_LITERAL("@");
		BUF_PUSH_BUF(*host);
		BUF_PUSH_LITERAL("/");
		BUF_PUSH_BUF(participant->nick);
		BUF_PUSH_LITERAL("'/>");
	}

	return TRUE;
}

BOOL build_room_info(BuilderBuffer *buffer, Room *room, Buffer *host) {
	int chunk_size;

	LDEBUG("building room info");
	BUF_PUSH_LITERAL("<identity category='conference' type='text' name='");
	if (room->title.size) {
		BUF_PUSH_BUF(room->title);
	} else {
		BUF_PUSH_BUF(room->node);
	}
	BUF_PUSH_LITERAL("'/><feature var='http://jabber.org/protocol/muc'/>");
	
	// TODO(artem): build items depending on room flags
	/*
		"<feature var='muc_public'/>"
		"<feature var='muc_persistent'/>"
		"<feature var='muc_open'/>"
		"<feature var='muc_semianonymous'/>"
		"<feature var='muc_moderated'/>"
		"<feature var='muc_unsecured'/>"

		See: http://xmpp.org/extensions/xep-0045.html#registrar-features
	*/

	return TRUE;
}

BOOL build_component_items(BuilderBuffer *buffer, Rooms *rooms, Buffer *host) {
	int chunk_size;
	Room *room = rooms->start;

	LDEBUG("building component items (list of rooms)");
	for (; room; room = room->next) {
		BUF_PUSH_LITERAL("<item name='");
		if (room->title.size) {
			BUF_PUSH_BUF(room->title);
		} else {
			BUF_PUSH_BUF(room->node);
		}
		BUF_PUSH_FMT(" (%d)", room->participants_count);
		BUF_PUSH_LITERAL("' jid='");
		BUF_PUSH_BUF(room->node);
		BUF_PUSH_LITERAL("@");
		BUF_PUSH_BUF(*host);
		BUF_PUSH_LITERAL("'/>");
	}

	return TRUE;
}

BOOL build_room_affiliations(BuilderBuffer *buffer, AffiliationEntry *aff, int affiliation) {
	int chunk_size;

	LDEBUG("building room affiliations");
	for (; aff; aff = aff->next) {
		BUF_PUSH_LITERAL("<item affiliation='");
		BUF_PUSH_STR(affiliation_names[affiliation]);
		BUF_PUSH_LITERAL("' jid='");
		BUF_PUSH(JID_STR(&aff->jid), JID_LEN(&aff->jid));
		if (!BUF_EMPTY(&aff->reason)) {
			BUF_PUSH_LITERAL("'><reason>");
			BUF_PUSH_BUF(aff->reason);
			BUF_PUSH_LITERAL("</reason></item>");
		} else {
			BUF_PUSH_LITERAL("'><reason/></item>");
		}
	}

	return TRUE;
}

BOOL build_iq_time(BuilderBuffer *buffer) {
	struct tm tm;
	time_t tm_t;
	char str_buffer[40];
	int chunk_size;

	LDEBUG("building iq:time response");

	time(&tm_t);
	localtime_r(&tm_t, &tm);
	if (strftime(str_buffer, sizeof(str_buffer), "%z", &tm) == 5) {
		// +0300 => +03:00
		memcpy(str_buffer+4, str_buffer+3, 3);
		str_buffer[3] = ':';
	}

	BUF_PUSH_LITERAL("<tzo>");
	BUF_PUSH_STR(str_buffer);
	BUF_PUSH_LITERAL("</tzo><utc>");

	gmtime_r(&tm_t, &tm);
	strftime(str_buffer, sizeof(str_buffer), "%Y-%m-%dT%T", &tm);

	BUF_PUSH_STR(str_buffer);
	BUF_PUSH_LITERAL("Z</utc>");
	return TRUE;
}

BOOL build_error(XMPPError *error, BuilderBuffer *buffer) {
	int chunk_size;

	LDEBUG("building type=error stanza");

	BUF_PUSH_LITERAL("<error code='");
	BUF_PUSH_STR(error->code);
	BUF_PUSH_LITERAL("' type='");
	BUF_PUSH_STR(error->type);
	BUF_PUSH_LITERAL("'><");
	BUF_PUSH_STR(error->name);
	BUF_PUSH_LITERAL(" xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/>"
			"<text xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'>");
	BUF_PUSH_STR(error->text);
	BUF_PUSH_LITERAL("</text></error>");

	return TRUE;
}

BOOL builder_build(BuilderPacket *packet, BuilderBuffer *buffer) {
	int chunk_size;

	LDEBUG("building packet: started");

	BUF_PUSH_BPT(packet->header);
	BUF_PUSH_LITERAL(" from='");
	if (!BUF_NULL(&packet->from_node)) {
		BUF_PUSH_BUF(packet->from_node);
		BUF_PUSH_LITERAL("@");
	}
	BUF_PUSH_BUF(packet->from_host);
	if (!BUF_NULL(&packet->from_nick)) {
		BUF_PUSH_LITERAL("/");
		BUF_PUSH_BUF(packet->from_nick);
	}
	BUF_PUSH_LITERAL("' to='");
	BUF_PUSH(JID_STR(&packet->to), JID_LEN(&packet->to));

	if (packet->type) {
		BUF_PUSH_LITERAL("' type='");
		switch (packet->type) {
			case 'g':
				if (packet->name == 'm') {
					BUF_PUSH_LITERAL("groupchat");
				} else {
					// iq
					BUF_PUSH_LITERAL("get");
				}
				break;
			case 'u':
				BUF_PUSH_LITERAL("unavailable");
				break;
			case 'c':
				BUF_PUSH_LITERAL("chat");
				break;
			case 'r':
				BUF_PUSH_LITERAL("result");
				break;
			case 'e':
				BUF_PUSH_LITERAL("error");
				break;
			case 's':
				BUF_PUSH_LITERAL("set");
				break;
		}
	}

	BUF_PUSH_LITERAL("'>");
	
	LDEBUG("header composed successfully");

	BUF_PUSH_IFBPT(packet->user_data);

	if (packet->type == 'e') {
		if (!build_error(packet->error, buffer)) {
			return FALSE;
		}
	} else {
		if (packet->name == 'p') {
			if (!build_presence_mucadm(&packet->participant, buffer)) {
				return FALSE;
			}
		} else if (packet->name == 'i') {
			switch (packet->iq_type) {
				case BUILD_IQ_VERSION:
					BUF_PUSH_LITERAL(
							"<query xmlns='jabber:iq:version'>"
								"<name>Mukite http://mukite.org/</name>"
								"<version>git</version>"
								"<os>Windows-XP 5.01.2600</os>"
							"</query>");
					break;
				case BUILD_IQ_LAST:
					BUF_PUSH_LITERAL("<query xmlns='jabber:iq:last' seconds='");
					BUF_PUSH_FMT("%.0f", packet->iq_last.seconds);
					BUF_PUSH_LITERAL("'/>");
					break;
				case BUILD_IQ_TIME:
					BUF_PUSH_LITERAL("<time xmlns='urn:xmpp:time'>");
					if (!build_iq_time(buffer)) {
						return FALSE;
					}
					BUF_PUSH_LITERAL("</time>");
					break;
				case BUILD_IQ_STATS:
					BUF_PUSH_LITERAL("<query xmlns='http://jabber.org/protocol/stats'>");
					if (!build_stats(buffer, packet)) {
						return FALSE;
					}
					BUF_PUSH_LITERAL("</query>");
					break;
				case BUILD_IQ_DISCO_INFO:
				case BUILD_IQ_ROOM_DISCO_INFO:
					BUF_PUSH_LITERAL("<query xmlns='http://jabber.org/protocol/disco#info'>");
					if (packet->iq_type > BUILD_IQ_ROOM) {
						if (!build_room_info(buffer, packet->room, &packet->from_host)) {
							return FALSE;
						}
					} else {
						BUF_PUSH_LITERAL(
								"<identity category='conference' type='text' name='Chatrooms'/>"
								"<feature var='http://jabber.org/protocol/disco#info'/>"
								"<feature var='http://jabber.org/protocol/disco#items'/>"
								"<feature var='http://jabber.org/protocol/muc'/>"
								"<feature var='jabber:iq:register'/>"
								"<feature var='jabber:iq:last'/>"
								"<feature var='jabber:iq:version'/>"
								"<feature var='urn:xmpp:time'/>");
					}
					BUF_PUSH_LITERAL("</query>");
					break;
				case BUILD_IQ_DISCO_ITEMS:
				case BUILD_IQ_ROOM_DISCO_ITEMS:
					BUF_PUSH_LITERAL("<query xmlns='http://jabber.org/protocol/disco#items'>");
					if (packet->iq_type > BUILD_IQ_ROOM) {
						if (!build_room_items(buffer, packet->room, &packet->from_host)) {
							return FALSE;
						}
					} else {
						if (!build_component_items(buffer, packet->rooms, &packet->from_host)) {
							return FALSE;
						}
					}
					BUF_PUSH_LITERAL("</query>");
					break;
				case BUILD_IQ_ROOM_AFFILIATIONS:
					BUF_PUSH_LITERAL("<query xmlns='http://jabber.org/protocol/muc#admin'>");
					if (!build_room_affiliations(buffer, packet->muc_items.items, packet->muc_items.affiliation)) {
						return FALSE;
					}
					BUF_PUSH_LITERAL("</query>");
					break;
			}
		}
	}

	if (packet->delay) {
		BUF_PUSH_LITERAL(
				"<delay xmlns='urn:xmpp:delay' from='hurr@conference.durr.purr' stamp='2012-12-05T14:48:28Z'/>"
				"<x xmlns='jabber:x:delay' stamp='20121205T14:48:28'/>");
	}

	switch (packet->name) {
		case 'm':
			BUF_PUSH_LITERAL("</message>");
			break;
		case 'p':
			BUF_PUSH_LITERAL("</presence>");
			break;
		case 'i':
			BUF_PUSH_LITERAL("</iq>");
			break;
	}

	LDEBUG("building packet: finished");

	return TRUE;
}

BOOL builder_push_status_code(MucAdmNode *participant, int code) {
	if (participant->status_codes_count >= MAX_STATUS_CODES) {
		return FALSE;
	}
	participant->status_codes[participant->status_codes_count++] = code;
	return TRUE;
}
