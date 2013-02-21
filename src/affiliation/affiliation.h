#ifndef AFFILIATION_H
#define AFFILIATION_H

#include "jid.h"

#define MAX_AFFILIATION_REASON_SIZE 65000

typedef struct Affiliation {
	Jid jid;
	BufferPtr reason_node;
	struct Affiliation *prev, *next;
} Affiliation;

Affiliation *affiliation_init(Affiliation *);
Affiliation *affiliation_destroy(Affiliation *);

BOOL affiliation_set_reason(Affiliation *, BufferPtr *);

BOOL affiliation_serialize(Affiliation *, FILE *);
BOOL affiliation_deserialize(Affiliation *, FILE *);

#endif
