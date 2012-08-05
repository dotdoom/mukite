#include "jid.h"

#include "acl.h"

void acl_init(ACLEntry *list) {
	list->role = ACL_NORMAL;
	list->next = 0;
}

int acl_role(ACLEntry *list, Jid *jid) {
	for (; list; list = list->next) {
		if (jid_cmp(&list->jid, jid, JID_FULL | JID_CMP_NULLWC)) {
			return list->role;
		}
	}
	return 0;
}
