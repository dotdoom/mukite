#include <string.h>

#include "xmcomp/logger.h"
#include "room.h"

#include "router.h"

#define STATUS_SEMIANONYMOUS 100
#define STATUS_SELF_PRESENCE 110
#define STATUS_LOGGING_ENABLED 170
#define STATUS_NICKNAME_ENFORCED 210
#define STATUS_BANNED 301
#define STATUS_NICKNAME_CHANGED 303
#define STATUS_KICKED 307
#define STATUS_NONMEMBER_REMOVED 321

#define IQ_VERSION_RESULT "<query xmlns='jabber:iq:version'><name>Mukite http://mukite.org/</name><version>svn</version><os>Windows-XP 5.01.2600</os></query>"
#define IQ_LAST_RESULT "<query xmlns='jabber:iq:last' seconds='321898758'/>"
#define IQ_URN_TIME_RESULT "<time xmlns='urn:xmpp:time'><tzo>-06:00</tzo><utc>2006-12-19T17:58:35Z</utc></time>"
#define IQ_STATS_ITEMS_RESULT "<query xmlns='http://jabber.org/protocol/stats'><stat name='time/uptime'/></query>"
#define IQ_STATS_VALUES_RESULT "<query xmlns='http://jabber.org/protocol/stats'><stat name='time/uptime' units='seconds' value='3054635'/></query>"
#define IQ_DISCO_INFO_RESULT \
	"<identity category='conference' type='text' name='Play-Specific Chatrooms'/><identity category='directory' type='chatroom' name='Play-Specific Chatrooms'/>" \
	"<feature var='http://jabber.org/protocol/disco#info'/>" \
	"<feature var='http://jabber.org/protocol/disco#items'/>" \
	"<feature var='http://jabber.org/protocol/muc'/>" \
	"<feature var='jabber:iq:register'/>" \
	"<feature var='jabber:iq:search'/>" \
	"<feature var='jabber:iq:time'/>" \
	"<feature var='jabber:iq:version'/>"

void router_cleanup(IncomingPacket *packet) {
	int index;
	for (index = 0; index < MAX_ERASE_CHUNKS && packet->erase[index].data; ++index) {
		memset(packet->erase[index].data, ' ', BPT_SIZE(&packet->erase[index]));
		packet->erase[index].data = 0;
	}
}

int component_handle(RouterChunk *chunk) {
	return 0;
}

int router_process(RouterChunk *chunk) {
	IncomingPacket *packet = &chunk->packet;

	LDEBUG("routing packet:\n"
			" ** Stanza '%c' type '%c'\n"
			" ** '%.*s' -> '%.*s'\n"
			" ** Header: '%.*s'\n"
			" ** Data: '%.*s'",
			packet->name, packet->type,
			JID_LEN(&packet->real_from), JID_STR(&packet->real_from),
			JID_LEN(&packet->proxy_to), JID_STR(&packet->proxy_to),
			BPT_SIZE(&packet->header), packet->header.data,
			BPT_SIZE(&packet->inner), packet->inner.data);

	if (BPT_SIZE(&packet->proxy_to.node)) {
		return rooms_route(chunk);
	} else {
		return component_handle(chunk);
	}
}

int router_error(RouterChunk *chunk, XMPPError *error) {
	LDEBUG("routing an error %s: %s/%s: %s", error->code, error->type, error->name, error->text);
	return 0;
}
