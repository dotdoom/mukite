#ifndef XMCOMP_XMLFSM_H
#define XMCOMP_XMLFSM_H

#include "common.h"
#include "buffer.h"

#define XMLPARSE_FAILURE -1
#define XMLPARSE_SUCCESS 0
#define XMLNODE_EMPTY 1
#define XMLNODE_NOATTR 2

typedef struct {
	BufferPtr name, value;
} XmlAttr;

int xmlfsm_skip_node(BufferPtr *, BufferPtr *);
int xmlfsm_node_name(BufferPtr *, Buffer *);
int xmlfsm_next_attr(BufferPtr *, XmlAttr *);

// Helper functions based on above
typedef struct {
	BufferPtr buffer, node;
	Buffer node_name;
	char *node_start;
} XmlNodeTraverser;

BOOL xmlfsm_skip_attrs(BufferPtr *);
BOOL xmlfsm_skipto_attr(BufferPtr *buffer, char *attr_name, XmlAttr *attr);
BOOL xmlfsm_next_sibling(XmlNodeTraverser *);
BOOL xmlfsm_skipto_node(BufferPtr *buffer, char *node_name, BufferPtr *node);

#endif
