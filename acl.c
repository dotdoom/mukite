#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xmcomp/logger.h"

#include "jid.h"

#include "acl.h"

void acl_init(ACLConfig *acl) {
	acl->first = 0;
	pthread_rwlock_init(&acl->sync, 0);
}

int acl_role(ACLConfig *acl, Jid *jid) {
	ACLEntry *entry = 0;
	int role;

	pthread_rwlock_rdlock(&acl->sync);

	role = acl->default_role;
	for (entry = acl->first; entry; entry = entry->next) {
		if (!jid_cmp(&entry->jid, jid, JID_FULL | JID_CMP_NULLWC)) {
			role = entry->role;
			break;
		}
	}

	pthread_rwlock_unlock(&acl->sync);

	return role;
}

BOOL acl_serialize(ACLConfig *acl, FILE *output) {
	ACLEntry *entry = 0;
	
	pthread_rwlock_rdlock(&acl->sync);
	for (entry = acl->first; entry; entry = entry->next) {
		if (fprintf(output, "%d %s\n", entry->role, JID_STR(&entry->jid)) <= 0) {
			return FALSE;
		}
	}
	pthread_rwlock_unlock(&acl->sync);

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
	BOOL result = TRUE;

	pthread_rwlock_wrlock(&acl->sync);

	// ACLs are special. We do not care about destruction of existing items in other data types -
	// we know their deserializers are only called once, on the application startup.
	// But ACLs may change while application works, thus we should care about existing items, too.
	acl->first = 0;
	for (; prev; prev = current) {
		current = prev->next;
		acl_entry_destroy(prev);
	}

	prev = 0;
	while (!feof(input)) {
		if (limit <= 0) {
			LERROR("acl count limit exceeded");
			result = FALSE;
			break;
		}
		jid_buffer.data = malloc(MAX_JID_SIZE+1);
		if (fscanf(input, "%d %3071[^\n]\n", &role, jid_buffer.data) != 2) {
			LERROR("invalid acl file format");
			result = FALSE;
			break;
		}
		current = malloc(sizeof(*current));
		current->role = role;
		jid_buffer.end = jid_buffer.data + strlen(jid_buffer.data);
		if (!jid_struct(&jid_buffer, &current->jid)) {
			LERROR("invalid JID format in the acl file");
			result = FALSE;
			break;
		}
		if (prev) {
			prev->next = current;
			prev = current;
		} else {
			prev = acl->first = current;
		}
		--limit;
		LINFO("ACL added: '%.*s' (%d)", JID_LEN(&current->jid), JID_STR(&current->jid), current->role);
	}

	pthread_rwlock_unlock(&acl->sync);
	return result;
}
