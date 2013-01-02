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
	XmlAttr xmlns_attr;
	XmlNodeTraverser nodes = { .buffer = ingress->inner };

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
			while (xmlfsm_next_sibling(&nodes)) {
				if (!xmlfsm_skipto_attr(&nodes.node, "xmlns", &xmlns_attr)) {
					continue;
				}
				xmlfsm_skip_attrs(&nodes.node);

				if (BUF_EQ_LIT("query", &nodes.node_name)) {
					if (BPT_EQ_LIT("jabber:iq:version", &xmlns_attr.value)) {
						egress->iq_type = BUILD_IQ_VERSION;
						egress->uname = &chunk->config->uname;
					} else if (BPT_EQ_LIT("jabber:iq:last", &xmlns_attr.value)) {
						egress->iq_type = BUILD_IQ_LAST;
						egress->iq_last.seconds = chunk->config->timer_thread.ticks / TIMER_RESOLUTION;
					} else if (BPT_EQ_LIT("http://jabber.org/protocol/stats", &xmlns_attr.value)) {
						egress->iq_type = BUILD_IQ_STATS;
						egress->iq_stats.request = nodes.node;
						egress->iq_stats.rooms = &chunk->config->rooms;
						egress->iq_stats.ringbuffer = &chunk->config->writer_thread.ringbuffer.stats;
						egress->iq_stats.queue = &chunk->config->reader_thread.queue.stats;
					} else if (BPT_EQ_LIT("http://jabber.org/protocol/disco#info", &xmlns_attr.value)) {
						egress->iq_type = BUILD_IQ_DISCO_INFO;
					} else if (BPT_EQ_LIT("http://jabber.org/protocol/disco#items", &xmlns_attr.value)) {
						egress->rooms = &chunk->config->rooms;
						egress->iq_type = BUILD_IQ_DISCO_ITEMS;
					}
				} else if (BUF_EQ_LIT("time", &nodes.node_name) &&
					BPT_EQ_LIT("urn:xmpp:time", &xmlns_attr.value)) {
					egress->iq_type = BUILD_IQ_TIME;
				} else if (BUF_EQ_LIT("vCard", &nodes.node_name)) {
					egress->iq_type = BUILD_IQ_VCARD;
				}

				if (egress->iq_type) {
					SEND(&chunk->send);
					break;
				} // TODO(artem): else send feature-not-implemented
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

	if (BPT_NULL(&ingress->proxy_to.node)) {
		component_handle(chunk);
	} else {
		rooms_route(chunk);
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
	SEND(&chunk->send);
	jid_destroy(&egress->to);
}
