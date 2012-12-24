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

typedef struct {
	BufferPtr buffer, node;
	Buffer node_name;
	char *node_start;
} XmlNodeTraverser;

int xmlfsm_skip_node(BufferPtr *, int, BufferPtr *);
int xmlfsm_node_name(BufferPtr *, Buffer *);
int xmlfsm_get_attr(BufferPtr *, XmlAttr *);

BOOL xmlfsm_skip_attrs(BufferPtr *);
BOOL xmlfsm_skipto_attr(BufferPtr *, char *, XmlAttr *);
BOOL xmlfsm_traverse_node(XmlNodeTraverser *);

#endif
