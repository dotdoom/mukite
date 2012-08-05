#ifndef ACL_H
#define ACL_H

#define ACL_NORMAL 0
#define ACL_MUC_CREATE 1
#define ACL_MUC_ADMIN 3
#define ACL_COMPONENT_ADMIN 7

typedef struct ACLEntry_t {
	Jid jid;
	int role;
	struct ACLEntry_t *next;
} ACLEntry;

void acl_init(ACLEntry *list);
int acl_role(ACLEntry *list, Jid *);

#endif
