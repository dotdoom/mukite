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
