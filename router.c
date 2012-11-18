#include <string.h>

#include "xmcomp/logger.h"
#include "room.h"

#include "router.h"

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
	IncomingPacket *ingress = &chunk->ingress;
	BuilderPacket *egress = &chunk->egress;
	BufferPtr buffer = ingress->inner, node = buffer;
	Buffer node_name;
	XmlAttr xmlns_attr;
	BOOL xmlns_found;
	struct tm tm;
	time_t tm_t;

	if (ingress->name != 'i') {
		// We do not handle <message> or <presence> to the nodeless JID
		return;
	}

	jid_cpy(&egress->to, &ingress->real_from, JID_FULL);
	router_cleanup(&chunk->ingress);
	egress->from_nick.data = 0;
	egress->type = 'r';

	switch (ingress->type) {
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
						egress->iq_type = BUILD_IQ_VERSION;
					} else if (BPT_EQ_LIT("jabber:iq:last", &xmlns_attr.value)) {
						egress->iq_type = BUILD_IQ_LAST;
						egress->iq_last.seconds = difftime(time(0), chunk->startup);
					} else if (BPT_EQ_LIT("http://jabber.org/protocol/stats", &xmlns_attr.value)) {
						//.iq_type = BUILD_IQ_STATS;
					} else if (BPT_EQ_LIT("http://jabber.org/protocol/disco#info", &xmlns_attr.value)) {
						egress->iq_type = BUILD_IQ_DISCO_INFO;
					} else if (BPT_EQ_LIT("http://jabber.org/protocol/disco#items", &xmlns_attr.value)) {
						egress->rooms = chunk->rooms;
						egress->iq_type = BUILD_IQ_DISCO_ITEMS;
					}
				}

				if (BUF_EQ_LIT("time", &node_name) &&
					BPT_EQ_LIT("urn:xmpp:time", &xmlns_attr.value)) {
					egress->iq_type = BUILD_IQ_TIME;
					time(&tm_t);
					localtime_r(&tm_t, &tm);
					strftime(egress->iq_time.tzo, sizeof(egress->iq_time.tzo), "%z", &tm);
					gmtime_r(&tm_t, &tm);
					strftime(egress->iq_time.utc, sizeof(egress->iq_time.utc), "%Y-%m-%dT%T", &tm);
					strcat(egress->iq_time.utc, "Z");
				}

				if (egress->iq_type) {
					chunk->send.proc(chunk->send.data);
					break;
				}
			}
			break;
	}

	jid_destroy(&egress->to);
}

void router_process(RouterChunk *chunk) {
	IncomingPacket *ingress = &chunk->ingress;
	BuilderPacket *egress = &chunk->egress;

	memset(egress, 0, sizeof(*egress));
	egress->name = ingress->name;
	egress->type = ingress->type;
	egress->header = ingress->header;
	egress->from_host = chunk->hostname;

	LDEBUG("routing packet:\n"
			" ** Stanza '%c' type '%c'\n"
			" ** '%.*s' -> '%.*s'\n"
			" ** Header: '%.*s'\n"
			" ** Data: '%.*s'",
			ingress->name, ingress->type,
			JID_LEN(&ingress->real_from), JID_STR(&ingress->real_from),
			JID_LEN(&ingress->proxy_to), JID_STR(&ingress->proxy_to),
			BPT_SIZE(&ingress->header), ingress->header.data,
			BPT_SIZE(&ingress->inner), ingress->inner.data);

	if (BPT_SIZE(&ingress->proxy_to.node)) {
		rooms_route(chunk);
	} else {
		component_handle(chunk);
	}
}

void router_error(RouterChunk *chunk, XMPPError *error) {
	IncomingPacket *ingress = &chunk->ingress;
	BuilderPacket *egress = &chunk->egress;

	jid_cpy(&egress->to, &ingress->real_from, JID_FULL);
	router_cleanup(ingress);
	egress->from_nick.data = 0;
	egress->type = 'e';
	egress->error = error;
	egress->user_data = ingress->inner;
	chunk->send.proc(chunk->send.data);
	jid_destroy(&egress->to);
}
