#include <string.h>

#include "xmcomp/src/logger.h"

#include "affiliations.h"

const char* affiliation_names[] = {
    "outcast",
    "none",
    "member",
    "admin",
    "owner"
};
const int affiliation_name_sizes[] = {
    7,
    4,
    6,
    5,
    5
};

inline Affiliation *affiliations_find_by_jid(AffiliationsList *affiliations, Jid *jid, int mode) {
	Affiliation *current = 0;
	DLS_FOREACH(affiliations, current) {
		if (!jid_cmp(&current->jid, jid, mode)) {
			return current;
		}
	}
	return 0;
}

int affiliationss_get_by_jid(AffiliationsList **affiliations, ACLConfig *acl, Jid *jid) {
	if (acl_role(acl, jid) >= ACL_MUC_ADMIN) {
		return AFFIL_OWNER;
	}

	// First search by full JID (node+host).
	for (int affiliation = AFFIL_OUTCAST; affiliation <= AFFIL_OWNER; ++affiliation) {
		if (affiliations_find_by_jid(affiliations[affiliation], jid, JID_NODE | JID_HOST)) {
			return affiliation;
		}
	}

	// Then search by host only.
	for (int affiliation = AFFIL_OUTCAST; affiliation <= AFFIL_OWNER; ++affiliation) {
		if (affiliations_find_by_jid(affiliations[affiliation], jid, JID_HOST)) {
			return affiliation;
		}
	}

	return AFFIL_NONE;
}

BOOL affiliations_serialize(AffiliationsList *affiliations, FILE *output) {
	DLS_SERIALIZE(affiliations, Affiliation, affiliation_serialize);
	return TRUE;
}

BOOL affiliations_deserialize(AffiliationsList *affiliations, FILE *input) {
	DLS_DESERIALIZE(affiliations, Affiliation, affiliation_deserialize);
	return TRUE;
}

BOOL affiliationss_add(AffiliationsList **affiliations, Participant *sender, int affiliation,
		Jid *jid, BufferPtr *reason_node) {
	// Only owner may ADD an affiliation of admin or higher.
	if (sender && sender->affiliation != AFFIL_OWNER && affiliation >= AFFIL_ADMIN) {
		return FALSE;
	}

	// Find an existing affiliation.
	Affiliation *affiliation_entry = 0;
	int list;
	for (list = AFFIL_OUTCAST; list <= AFFIL_OWNER; ++list) {
		if ((affiliation_entry = affiliations_find_by_jid(affiliations[list], jid, JID_NODE | JID_HOST))) {
			break;
		}
	}

	if (affiliation_entry) {
		// Only owner may CHANGE an affiliation of admin or higher.
		if (sender && sender->affiliation != AFFIL_OWNER && list >= AFFIL_ADMIN) {
			return FALSE;
		}
		if (list == AFFIL_OWNER &&
				sender &&
				jid_cmp(&sender->jid, &affiliation_entry->jid, JID_NODE | JID_HOST) == 0 &&
				affiliations[AFFIL_OWNER]->head == affiliation_entry &&
				!affiliation_entry->next) {
			// Owner trying to revoke own privilege when there are no other owners.
			return FALSE;
		}

		DLS_DELETE(affiliations[list], affiliation_entry);

		if (affiliation == AFFIL_NONE) {
			free(affiliation_destroy(affiliation_entry));
			affiliation_entry = 0;
		}
	}

	if (affiliation != AFFIL_NONE) {
		if (!affiliation_entry) {
			affiliation_entry = affiliation_init(malloc(sizeof(*affiliation_entry)));
		}
		jid_set(&affiliation_entry->jid, jid, JID_NODE | JID_HOST);
		affiliation_set_reason(affiliation_entry, reason_node);
		DLS_APPEND(affiliations[affiliation], affiliation_entry);
	}

	LDEBUG("set affiliation of '%.*s' to %d",
			JID_LEN(jid), JID_STR(jid),
			affiliation);

	return TRUE;
}

int affiliation_by_name(BufferPtr *name) {
	for (int affiliation = AFFIL_OUTCAST; affiliation <= AFFIL_OWNER; ++affiliation) {
		if (BPT_EQ_BIN(affiliation_names[affiliation], name, affiliation_name_sizes[affiliation])) {
			return affiliation;
		}
	}
	return AFFIL_UNCHANGED;
}
