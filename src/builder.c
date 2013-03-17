#include <string.h>

#include "xmcomp/src/logger.h"

#include "jid.h"
#include "room/room.h"
#include "config.h"
#include "timer.h"

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


#define BUF_PUSH_STATUS(value, mask, literal) \
	if ((value) & (mask)) { \
		BUF_PUSH_LITERAL("<status code='" literal "'/>"); \
	}

static BOOL build_presence_mucadm(struct MucUserNode *node, BuilderBuffer *buffer) {
	int chunk_size;

	BUF_PUSH_LITERAL("<x xmlns='http://jabber.org/protocol/muc#user'><item affiliation='");
	BUF_PUSH(affiliation_names[node->item.affiliation], affiliation_name_sizes[node->item.affiliation]);
	BUF_PUSH_LITERAL("' role='");
	BUF_PUSH(role_names[node->item.role], role_name_sizes[node->item.role]);
	if (!JID_EMPTY(&node->item.jid)) {
		BUF_PUSH_LITERAL("' jid='");
		BUF_PUSH(JID_STR(&node->item.jid), JID_LEN(&node->item.jid));
	}
	if (!BPT_NULL(&node->item.nick)) {
		BUF_PUSH_LITERAL("' nick='");
		BUF_PUSH_BPT(node->item.nick);
	}

	if (BPT_NULL(&node->item.reason_node)) {
		BUF_PUSH_LITERAL("'/>");
	} else {
		BUF_PUSH_LITERAL("'>");
		BUF_PUSH_BPT(node->item.reason_node);
		BUF_PUSH_LITERAL("</item>");
	}

	BUF_PUSH_STATUS(node->status_codes, STATUS_NON_ANONYMOUS, "100") else
		BUF_PUSH_STATUS(node->status_codes, STATUS_SELF_PRESENCE, "110") else
		BUF_PUSH_STATUS(node->status_codes, STATUS_LOGGING_ENABLED, "170") else
		BUF_PUSH_STATUS(node->status_codes, STATUS_ROOM_CREATED, "201") else
		BUF_PUSH_STATUS(node->status_codes, STATUS_NICKNAME_ENFORCED, "210") else
		BUF_PUSH_STATUS(node->status_codes, STATUS_BANNED, "301") else
		BUF_PUSH_STATUS(node->status_codes, STATUS_NICKNAME_CHANGED, "303") else
		BUF_PUSH_STATUS(node->status_codes, STATUS_KICKED, "307") else
		BUF_PUSH_STATUS(node->status_codes, STATUS_NONMEMBER_REMOVED, "321");


	BUF_PUSH_IFBPT(node->resume);

	BUF_PUSH_LITERAL("</x>");
	return TRUE;
}

#define BUF_PUSH_STAT(name, value, units) \
	BUF_PUSH_LITERAL("<stat name='" name "' units='" units "' value='"); \
	BUF_PUSH_FMT("%d", value); \
	BUF_PUSH_LITERAL("'/>");

static BOOL build_stats(BuilderBuffer *buffer, StanzaQueueStats *queue, RingBufferStats *ringbuffer, Rooms *rooms) {
	int chunk_size;

	//BUF_PUSH_STAT("rooms/count", rooms->size, "items");
	BUF_PUSH_STAT("queue/overflows", queue->overflows, "times");
	BUF_PUSH_STAT("queue/underflows", queue->underflows, "times");
	BUF_PUSH_STAT("queue/realloc_enlarges", queue->realloc_enlarges, "times");
	BUF_PUSH_STAT("queue/realloc_shortens", queue->realloc_shortens, "times");
	BUF_PUSH_STAT("queue/mallocs", queue->mallocs, "times");
	BUF_PUSH_STAT("queue/data_pushes", queue->data_pushes, "times");
	BUF_PUSH_STAT("queue/data_pops", queue->data_pops, "times");
	BUF_PUSH_STAT("queue/free_pushes", queue->free_pushes, "times");
	BUF_PUSH_STAT("queue/free_pops", queue->free_pops, "times");
	BUF_PUSH_STAT("ringbuffer/underflows", ringbuffer->underflows, "times");
	BUF_PUSH_STAT("ringbuffer/overflows", ringbuffer->overflows, "times");
	BUF_PUSH_STAT("ringbuffer/reads", ringbuffer->reads, "times");
	BUF_PUSH_STAT("ringbuffer/writes", ringbuffer->writes, "times");
	BUF_PUSH_STAT("time/uptime", timer_ticks() / TIMER_RESOLUTION, "seconds");

	/*

<stat name='muc/users'/>
<stat name='muc/jids'/>
	 */

	return TRUE;
}

BOOL build_room_items(BuilderBuffer *buffer, Room *room, BufferPtr *hostname) {
	int chunk_size;

	LDEBUG("building room items");
	Participant *current = 0;
	DLS_FOREACH(&room->participants, current) {
		BUF_PUSH_LITERAL("<item name='");
		BUF_PUSH_BPT(current->nick);
		BUF_PUSH_LITERAL("' jid='");
		BUF_PUSH_BUF(room->node);
		BUF_PUSH_LITERAL("@");
		BUF_PUSH_BPT(*hostname);
		BUF_PUSH_LITERAL("/");
		BUF_PUSH_BPT(current->nick);
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

BOOL build_room_info(BuilderBuffer *buffer, Room *room) {
	int chunk_size;

	LDEBUG("building room info");
	BUF_PUSH_LITERAL("<identity category='conference' type='text' name='");
	if (BUF_BLANK(&room->title)) {
		BUF_PUSH_BUF(room->node);
	} else {
		BUF_PUSH_BUF(room->title);
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

BOOL build_component_items(BuilderBuffer *buffer, Rooms *rooms, BufferPtr *hostname) {
	/*int chunk_size;
	Room *room = rooms->first;

	LDEBUG("building component items (list of rooms)");

	pthread_rwlock_rdlock(&rooms->sync);
	for (; room; room = room->next) {
		if (room->flags & MUC_FLAG_PUBLICROOM) {
			BUF_PUSH_LITERAL("<item name='");
			if (BUF_BLANK(&room->title)) {
				BUF_PUSH_BUF(room->node);
			} else {
				BUF_PUSH_BUF(room->title);
			}
			if (room->flags & MUC_FLAG_PUBLICPARTICIPANTS) {
				BUF_PUSH_FMT(" (%d)", room->participants.size);
			}
			BUF_PUSH_LITERAL("' jid='");
			BUF_PUSH_BUF(room->node);
			BUF_PUSH_LITERAL("@");
			BUF_PUSH_BPT(*hostname);
			BUF_PUSH_LITERAL("'/>");
		}
	}
	pthread_rwlock_unlock(&rooms->sync);*/

	return TRUE;
}

BOOL build_room_affiliations(BuilderBuffer *buffer, Affiliation *aff, int affiliation) {
	int chunk_size;

	LDEBUG("building room affiliations");
	for (; aff; aff = aff->next) {
		BUF_PUSH_LITERAL("<item affiliation='");
		BUF_PUSH_STR(affiliation_names[affiliation]);
		BUF_PUSH_LITERAL("' jid='");
		BUF_PUSH(JID_STR(&aff->jid), JID_LEN(&aff->jid));
		if (BPT_NULL(&aff->reason_node)) {
			BUF_PUSH_LITERAL("'><reason/></item>");
		} else {
			BUF_PUSH_LITERAL("'>");
			BUF_PUSH_BPT(aff->reason_node);
			BUF_PUSH_LITERAL("</item>");
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
	BUF_PUSH_FIELD("boolean", "Allow visitors to send custom status and change nickname",
			"muc#roomconfig_allowvisitorpresence", BUF_PUSH_BOOL(room->flags & MUC_FLAG_VISITORPRESENCE), );
	BUF_PUSH_FIELD("boolean", "Enable built-in bot (MewCat)",
			"muc#roomconfig_mewcat", BUF_PUSH_BOOL(room->flags & MUC_FLAG_MEWCAT), );
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

	tm_t = timer_time();
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
		case STANZA_MESSAGE:
			LDEBUG("name = message");
			BUF_PUSH_LITERAL("<message");
			break;
		case STANZA_PRESENCE:
			LDEBUG("name = presence");
			BUF_PUSH_LITERAL("<presence");
			break;
		case STANZA_IQ:
			LDEBUG("name = iq");
			BUF_PUSH_LITERAL("<iq");
			break;
	}

	LDEBUG("header = '%.*s'", BPT_SIZE(&packet->header), packet->header.data);
	BUF_PUSH_IFBPT(packet->header);
	BUF_PUSH_LITERAL(" from='");
	if (!BUF_NULL(&packet->from_node)) {
		BUF_PUSH_BUF(packet->from_node);
		BUF_PUSH_LITERAL("@");
	}
	BUF_PUSH_BPT(packet->from_host);
	if (!BUF_NULL(&packet->from_nick)) {
		BUF_PUSH_LITERAL("/");
		BUF_PUSH_BPT(packet->from_nick);
	}

	if (!JID_EMPTY(&packet->to)) {
		LDEBUG("to = '%.*s'", JID_LEN(&packet->to), JID_STR(&packet->to));
		BUF_PUSH_LITERAL("' to='");
		BUF_PUSH(JID_STR(&packet->to), JID_LEN(&packet->to));
	}

	if (packet->type) {
		BUF_PUSH_LITERAL("' type='");
		if (packet->type == STANZA_ERROR) {
			LDEBUG("type = error");
			BUF_PUSH_LITERAL("error");
		} else {
			switch (packet->name) {
				case STANZA_IQ:
					switch (packet->type) {
						case STANZA_IQ_GET:
							LDEBUG("type = get");
							BUF_PUSH_LITERAL("get");
							break;
						case STANZA_IQ_SET:
							LDEBUG("type = set");
							BUF_PUSH_LITERAL("set");
							break;
						case STANZA_IQ_RESULT:
							LDEBUG("type = result");
							BUF_PUSH_LITERAL("result");
							break;
					}
					break;
				case STANZA_MESSAGE:
					switch (packet->type) {
						case STANZA_MESSAGE_CHAT:
							LDEBUG("type = chat");
							BUF_PUSH_LITERAL("chat");
							break;
						case STANZA_MESSAGE_GROUPCHAT:
							LDEBUG("type = groupchat");
							BUF_PUSH_LITERAL("groupchat");
							break;
					}
					break;
				case STANZA_PRESENCE:
					switch (packet->type) {
						case STANZA_PRESENCE_UNAVAILABLE:
							LDEBUG("type = unavailable");
							BUF_PUSH_LITERAL("unavailable");
							break;
					}
					break;
			}
		}
	}

	BUF_PUSH_LITERAL("'>");
	
	LDEBUG("header composed successfully");

	BUF_PUSH_IFBPT(packet->user_data);

	if (packet->type == STANZA_ERROR) {
		if (packet->sys_data.error) {
			if (!build_error(packet->sys_data.error, buffer)) {
				return FALSE;
			}
		}
	} else {
		if (packet->name == STANZA_PRESENCE) {
			if (!build_presence_mucadm(&packet->sys_data.presence, buffer)) {
				return FALSE;
			}
		} else if (packet->name == STANZA_IQ) {
			switch (packet->iq_type) {
				case BUILD_IQ_EMPTY:
					break;
				case BUILD_IQ_VERSION:
					BUF_PUSH_LITERAL(
							"<query xmlns='jabber:iq:version'>"
								"<name>Mukite http://mukite.org/</name>"
								"<version>git</version>"
								"<os>");
					BUF_PUSH_STR(packet->sys_data.uname.sysname);
					BUF_PUSH_LITERAL("/");
					BUF_PUSH_STR(packet->sys_data.uname.machine);
					BUF_PUSH_LITERAL(" ");
					BUF_PUSH_STR(packet->sys_data.uname.release);
					BUF_PUSH_LITERAL("</os></query>");
					break;
				case BUILD_IQ_LAST:
					BUF_PUSH_LITERAL("<query xmlns='jabber:iq:last' seconds='");
					BUF_PUSH_FMT("%d", timer_ticks() / TIMER_RESOLUTION);
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
					if (!build_stats(buffer,
								packet->sys_data.stats.queue,
								packet->sys_data.stats.ringbuffer,
								packet->sys_data.stats.rooms)) {
						return FALSE;
					}
					BUF_PUSH_LITERAL("</query>");
					break;
				case BUILD_IQ_DISCO_INFO:
				case BUILD_IQ_ROOM_DISCO_INFO:
					BUF_PUSH_LITERAL("<query xmlns='http://jabber.org/protocol/disco#info'>");
					if (packet->iq_type > BUILD_IQ_ROOM) {
						if (!build_room_info(buffer, packet->sys_data.room)) {
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
						if (!build_room_items(buffer, packet->sys_data.room, &packet->from_host)) {
							return FALSE;
						}
					} else {
						if (!build_component_items(buffer, packet->sys_data.rooms, &packet->from_host)) {
							return FALSE;
						}
					}
					BUF_PUSH_LITERAL("</query>");
					break;
				case BUILD_IQ_ROOM_AFFILIATIONS:
					BUF_PUSH_LITERAL("<query xmlns='http://jabber.org/protocol/muc#admin'>");
					if (!build_room_affiliations(buffer, packet->sys_data.muc_items.items, packet->sys_data.muc_items.affiliation)) {
						return FALSE;
					}
					BUF_PUSH_LITERAL("</query>");
					break;
				case BUILD_IQ_ROOM_CONFIG:
					BUF_PUSH_LITERAL("<query xmlns='http://jabber.org/protocol/muc#owner'>");
					if (!build_room_config(buffer, packet->sys_data.room)) {
						return FALSE;
					}
					BUF_PUSH_LITERAL("</query>");
					break;
				case BUILD_IQ_VCARD:
					BUF_PUSH_LITERAL(
							"<vCard xmlns='vcard-temp'>"
								"<FN>Mukite XMPP Component</FN>"
								"<URL>http://mukite.org/</URL>"
							"</vCard>");
					break;
			}
		}
	}

	if (packet->delay) {
		BUF_PUSH_LITERAL("<delay xmlns='urn:xmpp:delay' from='");
		if (!BPT_NULL(&packet->from_node)) {
			BUF_PUSH_BUF(packet->from_node);
			BUF_PUSH_LITERAL("@");
		}
		BUF_PUSH_BPT(packet->from_host);
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
		case STANZA_MESSAGE:
			BUF_PUSH_LITERAL("</message>");
			break;
		case STANZA_PRESENCE:
			BUF_PUSH_LITERAL("</presence>");
			break;
		case STANZA_IQ:
			BUF_PUSH_LITERAL("</iq>");
			break;
	}

	LDEBUG("building packet: finished");

	return TRUE;
}
