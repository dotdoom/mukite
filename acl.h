#ifndef ACL_H
#define ACL_H

#define ACL_NORMAL 0
#define ACL_MUC_CREATE 1
#define ACL_MUC_ADMIN 3
#define ACL_COMPONENT_ADMIN 7

typedef struct ACLEntry {
	Jid jid;
	int role;
	struct ACLEntry *next;
} ACLEntry;

typedef struct {
	int default_role;
	ACLEntry *first;
} ACLConfig;

int acl_role(ACLConfig *acl, Jid *);

#endif
