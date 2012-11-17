#include <string.h>

#include "xmcomp/logger.h"
#include "room.h"

#include "router.h"

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
	IncomingPacket *input = &chunk->input;
	BuilderPacket *output = &chunk->output;
	BufferPtr buffer = input->inner, node = buffer;
	Buffer node_name;
	XmlAttr xmlns_attr;
	BOOL xmlns_found;
	struct tm tm;
	time_t tm_t;

	if (input->name != 'i') {
		// We do not handle <message> or <presence> to the nodeless JID
		return;
	}

	jid_cpy(&output->to, &input->real_from, JID_FULL);
	router_cleanup(&chunk->input);
	output->from_nick.data = 0;
	output->type = 'r';

	switch (input->type) {
		case 'g':
			for (; xmlfsm_skip_node(&buffer, 0, 0) == XMLPARSE_SUCCESS;
					node.data = buffer.data) {
				node.end = buffer.data;
				xmlfsm_node_name(&node, &node_name);

				xmlns_found = FALSE;
				while (xmlfsm_get_attr(&node, &xmlns_attr) == XMLPARSE_SUCCESS) {
					if (BPT_EQ_LIT("xmlns", &xmlns_attr.name)) {
						xmlns_found = TRUE;
						break;
					}
				}

				if (!xmlns_found) {
					// No use of <query> without xmlns="..."
					continue;
				}

				if (BUF_EQ_LIT("query", &node_name)) {
					if (BPT_EQ_LIT("jabber:iq:version", &xmlns_attr.value)) {
						output->iq_type = BUILD_IQ_VERSION;
					}
					if (BPT_EQ_LIT("jabber:iq:last", &xmlns_attr.value)) {
						output->iq_type = BUILD_IQ_LAST;
						output->iq_last.seconds = time(0) - chunk->startup;
					}
				}

				if (BUF_EQ_LIT("time", &node_name) &&
					BPT_EQ_LIT("urn:xmpp:time", &xmlns_attr.value)) {
					output->iq_type = BUILD_IQ_TIME;
					time(&tm_t);
					localtime_r(&tm_t, &tm);
					strftime(output->iq_time.tzo, sizeof(output->iq_time.tzo), "%z", &tm);
					gmtime_r(&tm_t, &tm);
					strftime(output->iq_time.utc, sizeof(output->iq_time.utc), "%Y-%m-%dT%T", &tm);
					strcat(output->iq_time.utc, "Z");
				}

				if (output->iq_type) {
					chunk->send.proc(chunk->send.data);
					break;
				}
			}
			break;
	}

	jid_destroy(&output->to);
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
