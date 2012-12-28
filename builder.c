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

#define BUF_PUSH_BOOL(value) \
	{ if (value) { BUF_PUSH_LITERAL("1"); } else { BUF_PUSH_LITERAL("0"); } }

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

	BUF_PUSH_LITERAL("'/>");
	BUF_PUSH_IFBPT(node->destroy_node);
	BUF_PUSH_LITERAL("</x>");
	return TRUE;
}

#define BUF_PUSH_STAT(category, value, units) \
	BUF_PUSH_LITERAL("<stat name='" #category "/" #value "' units='" units "' value='"); \
	BUF_PUSH_FMT("%d", data->iq_stats.category->value); \
	BUF_PUSH_LITERAL("'/>");

BOOL build_stats(BuilderBuffer *buffer, BuilderPacket *data) {
	int chunk_size;

	BUF_PUSH_STAT(rooms, size, "items");
	BUF_PUSH_STAT(queue, overflows, "times");
	BUF_PUSH_STAT(queue, underflows, "times");
	BUF_PUSH_STAT(queue, realloc_enlarges, "times");
	BUF_PUSH_STAT(queue, realloc_shortens, "times");
	BUF_PUSH_STAT(queue, mallocs, "times");
	BUF_PUSH_STAT(queue, data_pushes, "times");
	BUF_PUSH_STAT(queue, data_pops, "times");
	BUF_PUSH_STAT(queue, free_pushes, "times");
	BUF_PUSH_STAT(queue, free_pops, "times");
	BUF_PUSH_STAT(ringbuffer, underflows, "times");
	BUF_PUSH_STAT(ringbuffer, overflows, "times");
	BUF_PUSH_STAT(ringbuffer, reads, "times");

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
	ParticipantEntry *participant = room->participants.first;

	LDEBUG("building room items");
	for (; participant; participant = participant->next) {
		BUF_PUSH_LITERAL("<item name='");
		BUF_PUSH_BPT(participant->nick);
		BUF_PUSH_LITERAL("' jid='");
		BUF_PUSH_BUF(room->node);
		BUF_PUSH_LITERAL("@");
		BUF_PUSH_BUF(*host);
		BUF_PUSH_LITERAL("/");
		BUF_PUSH_BPT(participant->nick);
		BUF_PUSH_LITERAL("'/>");
	}

	return TRUE;
}

#define BUF_PUSH_FEATURE(test, if_true, if_false) \
	if (test) { \
		BUF_PUSH_LITERAL("<feature var='" if_true "'/>"); \
	} else { \
		BUF_PUSH_LITERAL("<feature var='" if_false "'/>"); \
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
	BUF_PUSH_LITERAL("'/>"
			"<feature var='http://jabber.org/protocol/muc'/>"
			"<feature var='http://jabber.org/protocol/disco#info'/>");
	if (room->flags & MUC_FLAG_PUBLICPARTICIPANTS) {
		BUF_PUSH_LITERAL("<feature var='http://jabber.org/protocol/disco#items'/>");
	}
	BUF_PUSH_FEATURE(room->flags & MUC_FLAG_PUBLICROOM, "muc_public", "muc_hidden");
	BUF_PUSH_FEATURE(room->flags & MUC_FLAG_PERSISTENTROOM, "muc_persistent", "muc_temporary");
	BUF_PUSH_FEATURE(room->flags & MUC_FLAG_MEMBERSONLY, "muc_membersonly", "muc_open");
	BUF_PUSH_FEATURE(room->flags & MUC_FLAG_SEMIANONYMOUS, "muc_semianonymous", "muc_nonanonymous");
	BUF_PUSH_FEATURE(room->flags & MUC_FLAG_MODERATEDROOM, "muc_moderated", "muc_unmoderated");
	BUF_PUSH_FEATURE(room->flags & MUC_FLAG_PASSWORDPROTECTEDROOM, "muc_passwordprotected", "muc_unsecured");

	return TRUE;
}

BOOL build_component_items(BuilderBuffer *buffer, Rooms *rooms, Buffer *host) {
	int chunk_size;
	Room *room = rooms->first;

	LDEBUG("building component items (list of rooms)");

	pthread_rwlock_rdlock(&rooms->sync);
	for (; room; room = room->next) {
		if (room->flags & MUC_FLAG_PUBLICROOM) {
			BUF_PUSH_LITERAL("<item name='");
			if (room->title.size) {
				BUF_PUSH_BUF(room->title);
			} else {
				BUF_PUSH_BUF(room->node);
			}
			if (room->flags & MUC_FLAG_PUBLICPARTICIPANTS) {
				BUF_PUSH_FMT(" (%d)", room->participants.size);
			}
			BUF_PUSH_LITERAL("' jid='");
			BUF_PUSH_BUF(room->node);
			BUF_PUSH_LITERAL("@");
			BUF_PUSH_BUF(*host);
			BUF_PUSH_LITERAL("'/>");
		}
	}
	pthread_rwlock_unlock(&rooms->sync);

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

#define BUF_PUSH_FIELD(type, label, var, data, appendix) \
	BUF_PUSH_LITERAL("<field type='" type "' label='" label "' var='" var "'><value>"); \
	data; \
	BUF_PUSH_LITERAL("</value>"); \
	appendix; \
	BUF_PUSH_LITERAL("</field>");

BOOL build_room_config(BuilderBuffer *buffer, Room *room) {
	int chunk_size;

	BUF_PUSH_LITERAL(
		"<instructions>You need an x:data capable client to configure room</instructions>"
		"<x xmlns='jabber:x:data' type='form'>"
			"<title>Configuration of room ");
	BUF_PUSH_BUF(room->node);
	BUF_PUSH_LITERAL("</title>");
	BUF_PUSH_FIELD("hidden", "", "FORM_TYPE",
			BUF_PUSH_LITERAL("http://jabber.org/protocol/muc#roomconfig"), );
	BUF_PUSH_FIELD("text-single", "Room title", "muc#roomconfig_roomname",
			BUF_PUSH_BUF(room->title), );
	BUF_PUSH_FIELD("text-single", "Room description", "muc#roomconfig_roomdesc",
			BUF_PUSH_BUF(room->description), );
	BUF_PUSH_FIELD("boolean", "Make room persistent", "muc#roomconfig_persistentroom",
			BUF_PUSH_BOOL(room->flags & MUC_FLAG_PERSISTENTROOM), );
	BUF_PUSH_FIELD("boolean", "Make room public searchable", "muc#roomconfig_publicroom",
			BUF_PUSH_BOOL(room->flags & MUC_FLAG_PUBLICROOM), );
	BUF_PUSH_FIELD("boolean", "Make participants list public", "public_list",
			BUF_PUSH_BOOL(room->flags & MUC_FLAG_PUBLICPARTICIPANTS), );
	BUF_PUSH_FIELD("boolean", "Make room password protected", "muc#roomconfig_passwordprotectedroom",
			BUF_PUSH_BOOL(room->flags & MUC_FLAG_PASSWORDPROTECTEDROOM), );
	BUF_PUSH_FIELD("text-private", "Password", "muc#roomconfig_roomsecret",
			BUF_PUSH_BUF(room->password), );
	BUF_PUSH_FIELD("list-single", "Maximum Number of Occupants", "muc#roomconfig_maxusers",
			BUF_PUSH_FMT("%d", room->participants.max_size),
			BUF_PUSH_LITERAL(
				"<option label='5'><value>5</value></option>"
				"<option label='10'><value>10</value></option>"
				"<option label='20'><value>20</value></option>"
				"<option label='30'><value>30</value></option>"
				"<option label='50'><value>50</value></option>"
				"<option label='100'><value>100</value></option>"
				"<option label='200'><value>200</value></option>"
				"<option label='500'><value>500</value></option>"
				"<option label='1000'><value>1000</value></option>"
			));
	BUF_PUSH_FIELD("list-single", "Present real Jabber IDs to", "muc#roomconfig_whois",
		if (room->flags & MUC_FLAG_SEMIANONYMOUS) {
			BUF_PUSH_LITERAL("moderators");
		} else {
			BUF_PUSH_LITERAL("anyone");
		}, BUF_PUSH_LITERAL(
				"<option label='moderators only'><value>moderators</value></option>"
				"<option label='anyone'><value>anyone</value></option>"
			));
	BUF_PUSH_FIELD("boolean", "Make room members-only", "muc#roomconfig_membersonly",
			BUF_PUSH_BOOL(room->flags & MUC_FLAG_MEMBERSONLY), );
	BUF_PUSH_FIELD("boolean", "Make room moderated","muc#roomconfig_moderatedroom",
			BUF_PUSH_BOOL(room->flags & MUC_FLAG_MODERATEDROOM), );
	BUF_PUSH_FIELD("boolean", "Default users as participants", "members_by_default",
			BUF_PUSH_BOOL(room->default_role == ROLE_PARTICIPANT), );
	BUF_PUSH_FIELD("boolean", "Allow users to change the subject", "muc#roomconfig_changesubject",
			BUF_PUSH_BOOL(room->flags & MUC_FLAG_CHANGESUBJECT), );
	BUF_PUSH_FIELD("boolean", "Allow users to send private messages", "allow_private_messages",
			BUF_PUSH_BOOL(room->flags & MUC_FLAG_ALLOWPM), );
	BUF_PUSH_FIELD("boolean", "Allow users to query other users", "allow_query_users",
			BUF_PUSH_BOOL(room->flags & MUC_FLAG_IQ_PROXY), );
	BUF_PUSH_FIELD("boolean", "Allow users to send invites", "muc#roomconfig_allowinvites",
			BUF_PUSH_BOOL(room->flags & MUC_FLAG_INVITES), );
	BUF_PUSH_FIELD("boolean", "Allow visitors to send private messages",
			"muc#roomconfig_allowvisitorspm", BUF_PUSH_BOOL(room->flags & MUC_FLAG_VISITORSPM), );
	BUF_PUSH_FIELD("boolean", "Allow visitors to send status text in presence updates",
			"muc#roomconfig_allowvisitorstatus", BUF_PUSH_BOOL(room->flags & MUC_FLAG_VISITORPRESENCE), );
	BUF_PUSH_LITERAL("</x>");

	return TRUE;
}

static BOOL build_strftime(BuilderBuffer *buffer, time_t *tm_t, BOOL utc_mark) {
	int chunk_size;
	struct tm tm;
	gmtime_r(tm_t, &tm);
	if (!(chunk_size = strftime(buffer->data_end, buffer->end - buffer->data_end,
					utc_mark ? "%Y-%m-%dT%T" : "%Y%m%dT%T", &tm))) {
		return FALSE;
	}
	buffer->data_end += chunk_size;
	if (utc_mark) {
		BUF_PUSH_LITERAL("Z");
	}
	return TRUE;
}

BOOL build_iq_time(BuilderBuffer *buffer) {
	time_t tm_t;
	struct tm tm;
	char str_buffer[10];
	int chunk_size;

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
	if (!build_strftime(buffer, &tm_t, TRUE)) {
		return FALSE;
	}
	BUF_PUSH_LITERAL("</utc>");
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

	switch (packet->name) {
		case 'm':
			BUF_PUSH_LITERAL("<message");
			break;
		case 'p':
			BUF_PUSH_LITERAL("<presence");
			break;
		case 'i':
			BUF_PUSH_LITERAL("<iq");
			break;
	}

	BUF_PUSH_IFBPT(packet->header);
	BUF_PUSH_LITERAL(" from='");
	if (!BUF_NULL(&packet->from_node)) {
		BUF_PUSH_BUF(packet->from_node);
		BUF_PUSH_LITERAL("@");
	}
	BUF_PUSH_BUF(packet->from_host);
	if (!BUF_NULL(&packet->from_nick)) {
		BUF_PUSH_LITERAL("/");
		BUF_PUSH_BPT(packet->from_nick);
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
				case BUILD_IQ_EMPTY:
					break;
				case BUILD_IQ_VERSION:
					BUF_PUSH_LITERAL(
							"<query xmlns='jabber:iq:version'>"
								"<name>Mukite http://mukite.org/</name>"
								"<version>git</version>"
								"<os>");
					BUF_PUSH_STR(packet->uname->sysname);
					BUF_PUSH_LITERAL("/");
					BUF_PUSH_STR(packet->uname->machine);
					BUF_PUSH_LITERAL(" ");
					BUF_PUSH_STR(packet->uname->release);
					BUF_PUSH_LITERAL("</os></query>");
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
								//"<feature var='jabber:iq:register'/>"
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
				case BUILD_IQ_ROOM_CONFIG:
					BUF_PUSH_LITERAL("<query xmlns='http://jabber.org/protocol/muc#owner'>");
					if (!build_room_config(buffer, packet->room)) {
						return FALSE;
					}
					BUF_PUSH_LITERAL("</query>");
					break;
			}
		}
	}

	if (packet->delay) {
		BUF_PUSH_LITERAL("<delay xmlns='urn:xmpp:delay' from='");
		if (!BUF_NULL(&packet->from_node)) {
			BUF_PUSH_BUF(packet->from_node);
			BUF_PUSH_LITERAL("@");
		}
		BUF_PUSH_BUF(packet->from_host);
		BUF_PUSH_LITERAL("' stamp='");
		if (!build_strftime(buffer, &packet->delay, TRUE)) {
			return FALSE;
		}
		BUF_PUSH_LITERAL("'/><x xmlns='jabber:x:delay' stamp='");
		if (!build_strftime(buffer, &packet->delay, FALSE)) {
			return FALSE;
		}
		BUF_PUSH_LITERAL("'/>");
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
