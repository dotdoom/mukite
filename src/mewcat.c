#include <string.h>

#include "xmcomp/src/logger.h"

#include "mewcat.h"

/*static int count_different_bytes(BufferPtr *buffer) {
	BOOL bytes[256];
	char *current;
	int i, total = 0;
	memset(bytes, 0, sizeof(bytes));
	for (current = buffer->data; current < buffer->end; ++current) {
		bytes[(unsigned char)(*current)] = TRUE;
	}
	for (i = 0; i < 256; ++i) {
		if (bytes[i]) {
			++total;
		}
	}
	LDEBUG("total %d chars", total);
	return total;
}

static BOOL adhoc_filter(BufferPtr *data) {
	BufferPtr inner = *data, body;
	Buffer node_name;
	if (xmlfsm_skipto_node(&inner, "body", &body)) {
		xmlfsm_node_name(&body, &node_name);
		if (xmlfsm_skip_attrs(&body)) {
			body.end -= sizeof("</body>") - 1;
			LDEBUG("body (%d): '%.*s'", BPT_SIZE(&body), BPT_SIZE(&body), body.data);
			if (BPT_SIZE(&body) > 10 &&
					count_different_bytes(&body) < 6) {
				BPT_SET_LIT(data, "<body>Happy New Year!</body>");
				return TRUE;
			}
		}
	}
	return TRUE;
}*/

BOOL mewcat_handle(Room *room,
		Participant *sender, Participant *receiver,
		BuilderPacket *egress) {
	return TRUE;
//	return (egress->name != STANZA_MESSAGE) ||
//		adhoc_filter(&egress->user_data);
}
