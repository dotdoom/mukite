#include <string.h>

#include "xmcomp/logger.h"
#include "room.h"

#include "router.h"

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
/*
 * ----11111-----3333-----222----
 * ------444444444444444--------- ???
 * (1) reordering
 *
 * 1<
 * 2>
 * 3>
 *
 * erase + move
 *
 * Move: data shift when larger indexes
 *
 *
 *
 */

	/*int i, j, active_chunks = 0;
	BufferPtr tmp;
	for (; active_chunks < MAX_ERASE_CHUNKS && packet->erase[active_chunks].data;
			++active_chunks)
		;

	for (i = 0; i < active_chunks-1; ++i) {
		for (j = i+1; j < active_chunks; ++j) {
			if (packet->erase[i].data < packet->erase[j].data) {
				tmp = packet->erase[i];
				packet->erase[i] = packet->erase[j];
				packet->erase[j] = tmp;
			}
		}
	}

	for (i = 0; i < active_chunks; ++i) {

	}*/
	


	int index;
	for (index = 0; index < MAX_ERASE_CHUNKS && packet->erase[index].data; ++index) {
		memset(packet->erase[index].data, ' ', BPT_SIZE(&packet->erase[index]));
		packet->erase[index].data = 0;
	}
}

void component_handle(RouterChunk *chunk) {
}

void router_process(RouterChunk *chunk) {
	IncomingPacket *input = &chunk->input;
	BuilderPacket *output = &chunk->output;

	memset(output, 0, sizeof(*output));
	output->name = input->name;
	output->type = input->type;
	output->header = input->header;
	output->from_host = chunk->hostname;

	LDEBUG("routing packet:\n"
			" ** Stanza '%c' type '%c'\n"
			" ** '%.*s' -> '%.*s'\n"
			" ** Header: '%.*s'\n"
			" ** Data: '%.*s'",
			input->name, input->type,
			JID_LEN(&input->real_from), JID_STR(&input->real_from),
			JID_LEN(&input->proxy_to), JID_STR(&input->proxy_to),
			BPT_SIZE(&input->header), input->header.data,
			BPT_SIZE(&input->inner), input->inner.data);

	if (BPT_SIZE(&input->proxy_to.node)) {
		rooms_route(chunk);
	} else {
		component_handle(chunk);
	}
}

void router_error(RouterChunk *chunk, XMPPError *error) {
	BuilderPacket *output = &chunk->output;

	jid_cpy(&output->to, &chunk->input.real_from, JID_FULL);
	router_cleanup(&chunk->input);
	output->from_nick.data = 0;
	output->type = 'e';
	output->error = error;
	output->user_data = chunk->input.inner;
	chunk->send.proc(chunk->send.data);
	jid_destroy(&output->to);
}
