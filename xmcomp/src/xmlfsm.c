#include <string.h>

#include "common.h"

#include "xmlfsm.h"

#define INC_UTF8(value, length) \
	switch ((value) & 0xFC) { \
		case 0xC0: length += 2; break; \
		case 0xE0: length += 3; break; \
		case 0xF0: length += 4; break; \
		case 0xF8: length += 5; break; \
		case 0xFC: length += 6; break; \
	}

int xmlfsm_skip_node(BufferPtr *buffer, BufferPtr *real_buffer) {
	char *tag_closing_pos = 0, *orig_start = buffer->data;
	int nesting_level = 0;
	char quote_char = 0;
	BOOL tag = FALSE;
	char *current_end =
		(real_buffer && buffer->data > buffer->end) ?
		real_buffer->end : // Wrap on the ring buffer border
		buffer->end;

	// All this assumes there's no such thing - '<![CDATA['
	for (;; ++buffer->data) {
		// Cycle the buffer if needed
		if (buffer->data == current_end) {
			if (current_end == buffer->end) {
				break;
			} else {
				buffer->data = real_buffer->data;
				current_end = buffer->end;
			}
		}

		if (quote_char) {
			// We aren't validating here. We just parse what we have.
			// Totally ignore XML within quotes.
			buffer->data = memchr(buffer->data, quote_char, current_end - buffer->data);
			if (buffer->data) {
				quote_char = 0;
			} else {
				buffer->data = current_end - 1;
			}
		} else {
			switch (*buffer->data) {
				case '<':
					++nesting_level;
					tag = TRUE;
					break;
				case '/':
					if (tag) {
						tag_closing_pos = buffer->data;
					}
					break;
				case '>':
					if (tag && tag_closing_pos) {
						--nesting_level;
						if (buffer->data - tag_closing_pos != 1) {
							// This is not an empty node
							--nesting_level;
						}

						if (!nesting_level) {
							++buffer->data;
							return XMLPARSE_SUCCESS;
						}
						tag_closing_pos = 0;
					}
					tag = FALSE;
					break;
				case '"':
				case '\'':
					quote_char = *buffer->data;
					break;
			}
		}
	}

	buffer->data = orig_start;
	return XMLPARSE_FAILURE;
}

#define WHITESPACE(c) \
	(c == ' ' || c == '\t' || c == '\n' || c == '\r')

#define SKIP(ptr, expr) \
	{ while (expr) { ++(ptr); } }

int xmlfsm_next_attr(BufferPtr *buffer, XmlAttr *attr) {
	char quote;
	attr->name.data = attr->name.end =
		attr->value.data = attr->value.end = 0;

	// Skip first whiteness
	SKIP(buffer->data, WHITESPACE(*buffer->data));

	// Detect end of attr list
	switch (*buffer->data) {
		case '/':
			buffer->data += 2;
			return XMLNODE_EMPTY;
		case '>':
			++buffer->data;
			return XMLNODE_NOATTR;
	}

	// Set attr name
	attr->name.data = buffer->data;
	SKIP(buffer->data, !WHITESPACE(*buffer->data) && *buffer->data != '=');
	attr->name.end = buffer->data;

	// Skip until quotes (XML attr values MUST be quoted)
	SKIP(buffer->data, *buffer->data != '"' && *buffer->data != '\'');

	// Get quote char and skip until the closing quote
	quote = *buffer->data;
	attr->value.data = ++buffer->data;
	SKIP(buffer->data, *buffer->data != quote);
	attr->value.end = buffer->data;

	// Skip the closing quote
	++buffer->data;

	return XMLPARSE_SUCCESS;
}

int xmlfsm_node_name(BufferPtr *buffer, Buffer *name) {
	buffer->data = memchr(buffer->data, '<', BPT_SIZE(buffer));

	for (name->data = ++buffer->data;
			!WHITESPACE(*buffer->data) && *buffer->data != '>' && *buffer->data != '/';
			++buffer->data) {
	}
	name->size = buffer->data - name->data;

	return XMLPARSE_SUCCESS;
}

BOOL xmlfsm_skip_attrs(BufferPtr *buffer) {
	XmlAttr attr;
	int last_get_attr_result;
	while ((last_get_attr_result = xmlfsm_next_attr(buffer, &attr)) == XMLPARSE_SUCCESS) {
		;
	}
	return last_get_attr_result == XMLNODE_NOATTR;
}

BOOL xmlfsm_skipto_attr(BufferPtr *buffer, char *attr_name, XmlAttr *attr) {
	int name_size = strlen(attr_name);
	while (xmlfsm_next_attr(buffer, attr) == XMLPARSE_SUCCESS) {
		if (BPT_EQ_BIN(attr_name, &attr->name, name_size)) {
			return TRUE;
		}
	}
	return FALSE;
}

BOOL xmlfsm_next_sibling(XmlNodeTraverser *data) {
	data->node.data = data->buffer.data;
	if (xmlfsm_skip_node(&data->buffer, 0) == XMLPARSE_SUCCESS) {
		data->node.end = data->buffer.data;
		data->node_start = data->node.data;
		xmlfsm_node_name(&data->node, &data->node_name);
		return TRUE;
	}
	return FALSE;
}

BOOL xmlfsm_skipto_node(BufferPtr *buffer, char *node_name, BufferPtr *node) {
	XmlNodeTraverser nodes = { .buffer = *buffer };
	int name_size = strlen(node_name);
	while (xmlfsm_next_sibling(&nodes)) {
		if (BUF_EQ_BIN(node_name, &nodes.node_name, name_size)) {
			node->data = nodes.node_start;
			node->end = nodes.node.end;
			*buffer = nodes.buffer;
			return TRUE;
		}
	}
	return FALSE;
}
