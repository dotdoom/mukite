#include <string.h>

#include "common.h"

#include "xmlfsm.h"

#define VALIDATE_END \
	{ if (buffer->data == buffer->end) { return XMLPARSE_FAILURE; } }

#define INC_UTF8(value, length) \
	switch ((value) & 0xFC) { \
		case 0xC0: length += 2; break; \
		case 0xE0: length += 3; break; \
		case 0xF0: length += 4; break; \
		case 0xF8: length += 5; break; \
		case 0xFC: length += 6; break; \
	}

int xmlfsm_skip_node(BufferPtr *buffer, int level, BufferPtr *real_buffer) {
	char *tag_closing_pos = 0, *orig_start = buffer->data;
	int nesting_level = 0;
	char quote_char = 0;
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
			if (*buffer->data == quote_char) {
				quote_char = 0;
			}
		} else {
			switch (*buffer->data) {
				case '<':
					++nesting_level;
					break;
				case '/':
					tag_closing_pos = buffer->data;
					break;
				case '>':
					if (tag_closing_pos) {
						--nesting_level;
						if (buffer->data - tag_closing_pos != 1) {
							// This is not an empty node
							--nesting_level;
						}

						if (nesting_level == level) {
							++buffer->data;
							return XMLPARSE_SUCCESS;
						}
						tag_closing_pos = 0;
					}
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

int xmlfsm_get_attr(BufferPtr *buffer, XmlAttr *attr) {
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
	for (name->data = 0; *buffer->data != '<'; ++buffer->data) {
		VALIDATE_END;
	}

	for (name->data = ++buffer->data; !WHITESPACE(*buffer->data) && *buffer->data != '>'; ++buffer->data) {
		VALIDATE_END;
	}
	name->size = buffer->data - name->data;

	if (*buffer->data == '>') {
		// Let xmlfsm_get_attr see the truth
		--buffer->data;
	}

	return XMLPARSE_SUCCESS;
}
