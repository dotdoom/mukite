#include <string.h>

#include "xmcomp/src/logger.h"

#include "room/room.h"
#include "builder.h"
#include "packet.h"
#include "worker.h"

static XMPPError error_definitions[] = {
	{
#define ERROR_NOT_IMPLEMENTED 0
		.code = "501",
		.name = "feature-not-implemented",
		.type = "cancel",
		.text = "This service does not provide the requested feature"
	}
};

void component_process(IncomingPacket *ingress) {
	BuilderPacket egress = {};
	XmlAttr xmlns_attr;
	XmlNodeTraverser nodes = { .buffer = ingress->inner };

	if (ingress->name != STANZA_IQ) {
		// We do not handle <message> or <presence> to the nodeless JID
		return;
	}

	packet_cleanup(ingress, PACKET_CLEANUP_TO | PACKET_CLEANUP_SWITCH_FROM_TO);

	egress.name = STANZA_IQ;
	egress.type = STANZA_IQ_RESULT;
	egress.header = ingress->header;

	switch (ingress->type) {
		case STANZA_IQ_GET:
			while (xmlfsm_next_sibling(&nodes)) {
				if (!xmlfsm_skipto_attr(&nodes.node, "xmlns", &xmlns_attr)) {
					continue;
				}
				xmlfsm_skip_attrs(&nodes.node);

				if (BUF_EQ_LIT("query", &nodes.node_name)) {
					if (BPT_EQ_LIT("jabber:iq:version", &xmlns_attr.value)) {
						egress.iq_type = BUILD_IQ_VERSION;
					} else if (BPT_EQ_LIT("jabber:iq:last", &xmlns_attr.value)) {
						egress.iq_type = BUILD_IQ_LAST;
					} else if (BPT_EQ_LIT("http://jabber.org/protocol/stats", &xmlns_attr.value)) {
						egress.iq_type = BUILD_IQ_STATS;
					} else if (BPT_EQ_LIT("http://jabber.org/protocol/disco#info", &xmlns_attr.value)) {
						egress.iq_type = BUILD_IQ_DISCO_INFO;
					} else if (BPT_EQ_LIT("http://jabber.org/protocol/disco#items", &xmlns_attr.value)) {
						egress.iq_type = BUILD_IQ_DISCO_ITEMS;
					}
				} else if (BUF_EQ_LIT("time", &nodes.node_name) &&
					BPT_EQ_LIT("urn:xmpp:time", &xmlns_attr.value)) {
					egress.iq_type = BUILD_IQ_TIME;
				} else if (BUF_EQ_LIT("vCard", &nodes.node_name)) {
					egress.iq_type = BUILD_IQ_VCARD;
				}

				if (!egress.iq_type) {
					egress.type = STANZA_ERROR;
					egress.sys_data.error = &error_definitions[ERROR_NOT_IMPLEMENTED];
				}
				worker_send(&egress);
				break;
			}
			break;
	}
}
