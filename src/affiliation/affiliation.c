#include <string.h>

#include "xmcomp/src/logger.h"

#include "acl.h"
#include "jid.h"

#include "affiliation.h"

BOOL affiliation_serialize(Affiliation *affiliation, FILE *output) {
	return
		jid_serialize(&affiliation->jid, output) &&
		buffer_ptr_serialize(&affiliation->reason_node, output);
}

BOOL affiliation_deserialize(Affiliation *affiliation, FILE *input) {
	return
        jid_deserialize(&affiliation->jid, input) &&
        buffer_ptr_deserialize(&affiliation->reason_node, input, MAX_AFFILIATION_REASON_SIZE);
}

Affiliation *affiliation_destroy(Affiliation *affiliation) {
	free(affiliation->reason_node.data);
	jid_destroy(&affiliation->jid);
	return affiliation;
}

Affiliation *affiliation_init(Affiliation *affiliation) {
	return memset(affiliation, 0, sizeof(*affiliation));
}

BOOL affiliation_set_reason(Affiliation *affiliation, BufferPtr *reason_node) {
	BPT_SETTER(affiliation->reason_node, reason_node, MAX_AFFILIATION_REASON_SIZE);
}
