#ifndef ACL_H
#define ACL_H

#include <pthread.h>

#define ACL_NORMAL 0
#define ACL_MUC_CREATE 1
#define ACL_MUC_PERSIST 2
#define ACL_MUC_ADMIN 3
#define ACL_COMPONENT_ADMIN 4

typedef struct ACLEntry {
	Jid jid;
	int role;
	struct ACLEntry *next;
} ACLEntry;

typedef struct {
	int default_role;
	ACLEntry *first;
	pthread_rwlock_t sync;
} ACLConfig;

void acl_init(ACLConfig *);
void acl_destroy(ACLConfig *);

int acl_role(ACLConfig *, Jid *);
BOOL acl_serialize(ACLConfig *, FILE *);
BOOL acl_deserialize(ACLConfig *, FILE *, int);

#endif
