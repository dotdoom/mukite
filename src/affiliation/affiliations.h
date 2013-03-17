#ifndef AFFILIATIONS_H
#define AFFILIATIONS_H

#include "ut2s.h"
#include "acl.h"
#include "participant/participant.h"
#include "affiliation.h"

extern const char* affiliation_names[];
extern const int affiliation_name_sizes[];

#define AFFIL_UNCHANGED -2
#define AFFIL_OUTCAST 0
#define AFFIL_NONE 1
#define AFFIL_MEMBER 2
#define AFFIL_ADMIN 3
#define AFFIL_OWNER 4

typedef struct {
	DLS_DECLARE(Affiliation);
} AffiliationsList;

int affiliationss_get_by_jid(AffiliationsList **, ACLConfig *, Jid *);
BOOL affiliationss_add(AffiliationsList **, Participant *, int affiliation, Jid *, BufferPtr *reason);
int affiliation_by_name(BufferPtr *);

BOOL affiliations_serialize(AffiliationsList *, FILE *);
BOOL affiliations_deserialize(AffiliationsList *, FILE *);

Affiliation *affiliations_find_by_jid(AffiliationsList *, Jid *, int mode);

#endif
