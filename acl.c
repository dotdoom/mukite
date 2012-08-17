#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xmcomp/logger.h"

#include "jid.h"

#include "acl.h"

int acl_role(ACLConfig *acl, Jid *jid) {
	ACLEntry *entry = acl->first;
	for (; entry; entry = entry->next) {
		if (!jid_cmp(&entry->jid, jid, JID_FULL | JID_CMP_NULLWC)) {
			return entry->role;
		}
	}
	return acl->default_role;
}

BOOL acl_serialize(ACLConfig *acl, FILE *output) {
	ACLEntry *entry = acl->first;

	for (; entry; entry = entry->next) {
		if (fprintf(output, "%d %s\n", entry->role, JID_STR(&entry->jid)) <= 0) {
			return FALSE;
		}
	}
	return TRUE;
}

void acl_entry_destroy(ACLEntry *entry) {
	jid_destroy(&entry->jid);
	free(entry);
}

BOOL acl_deserialize(ACLConfig *acl, FILE *input, int limit) {
	int role;
	BufferPtr jid_buffer;
	ACLEntry *prev = acl->first, *current = 0;

	acl->first = 0;
	for (; prev; prev = current) {
		current = prev->next;
		acl_entry_destroy(prev);
	}

	prev = 0;
	while (!feof(input)) {
		if (limit <= 0) {
			LERROR("acl count limit exceeded");
			return FALSE;
		}
		jid_buffer.data = malloc(MAX_JID_SIZE+1);
		if (fscanf(input, "%d %3071[^\n]\n", &role, jid_buffer.data) != 2) {
			LERROR("invalid acl file format");
			return FALSE;
		}
		current = malloc(sizeof(*current));
		current->role = role;
		jid_buffer.end = jid_buffer.data + strlen(jid_buffer.data);
		if (!jid_struct(&jid_buffer, &current->jid)) {
			LERROR("invalid JID format in the acl file");
			return FALSE;
		}
		if (prev) {
			prev->next = current;
			prev = current;
		} else {
			prev = acl->first = current;
		}
		--limit;
	}
	return TRUE;
}
