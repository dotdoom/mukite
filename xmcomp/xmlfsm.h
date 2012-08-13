#ifndef XMCOMP_XMLFSM_H
#define XMCOMP_XMLFSM_H

#include "common.h"
#include "buffer.h"

#define XMLPARSE_FAULT -1
#define XMLPARSE_SUCCESS 0
#define XMLNODE_EMPTY 1
#define XMLNODE_NOATTR 2

typedef struct {
	BufferPtr name, value;
} XmlAttr;

int xmlfsm_skip_node(BufferPtr *, int, BufferPtr *);
int xmlfsm_node_name(BufferPtr *, Buffer *);
int xmlfsm_get_attr(BufferPtr *, XmlAttr *);

#endif
