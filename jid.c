#include <stdlib.h>
#include <string.h>

#include "xmcomp/logger.h"

#include "jid.h"

// In this file 'strncasecmp' is used instead of 'memcmp' to provide case-insensitive JID comparison.
// Though 'strncasecmp' stops at NULL character,
// stringprep profile of JID prohibits it's usage, so we should be fine.

void jid_init(Jid *jid) {
	BPT_INIT(&jid->node);
	BPT_INIT(&jid->host);
	BPT_INIT(&jid->resource);
}

BOOL jid_empty(Jid *jid) {
	return !jid->host.data;
}

BOOL jid_struct(BufferPtr *jid_string, Jid *jid_struct) {
	int part = JID_NODE;
	char *part_start = jid_string->data,
		*current = jid_string->data;

	jid_init(jid_struct);
	for (; current < jid_string->end; ++current) {
		if (part == JID_NODE && *current == '@') {
			jid_struct->node.data = part_start;
			jid_struct->node.end = current;
			part = JID_HOST;
			part_start = current+1;
		} else if (*current == '/') {
			jid_struct->host.data = part_start;
			jid_struct->host.end = current;
			part = JID_RESOURCE;
			part_start = current+1;
			current = jid_string->end;
			break;
		}
	}

	if (part == JID_RESOURCE) {
		jid_struct->resource.data = part_start;
		jid_struct->resource.end = current;
	} else {
		jid_struct->host.data = part_start;
		jid_struct->host.end = current;
	}

	if (BPT_BLANK(&jid_struct->host) ||
			(BPT_SIZE(&jid_struct->host) > MAX_JID_PART_SIZE) ||
			(BPT_SIZE(&jid_struct->node) > MAX_JID_PART_SIZE) ||
			(BPT_SIZE(&jid_struct->resource) > MAX_JID_PART_SIZE)) {
		return FALSE;
	}

	return TRUE;
}

int jid_cmp(Jid *jid1, Jid *jid2, int mode) {
	int size;

	if ((mode & JID_NODE) && (jid1->node.data || !(mode & JID_CMP_NULLWC))) {
		if (!jid1->node.data != !jid2->node.data) {
			return 1;
		}

		if ((size = BPT_SIZE(&jid1->node)) != BPT_SIZE(&jid2->node)) {
			return 1;
		}

		if (strncasecmp(jid1->node.data, jid2->node.data, size)) {
			return 1;
		}
	}

	if (mode & JID_HOST) {
		if ((size = BPT_SIZE(&jid1->host)) != BPT_SIZE(&jid2->host)) {
			return 1;
		}

		if (strncasecmp(jid1->host.data, jid2->host.data, size)) {
			return 1;
		}
	}

	if ((mode & JID_RESOURCE) && (jid1->resource.data || !(mode & JID_CMP_NULLWC))) {
		if (!jid1->resource.data != !jid2->resource.data) {
			return 1;
		}

		if ((size = BPT_SIZE(&jid1->resource)) != BPT_SIZE(&jid2->resource)) {
			return 1;
		}

		if (strncasecmp(jid1->resource.data, jid2->resource.data, size)) {
			return 1;
		}
	}

	return 0;
}

int jid_strcmp(Jid *jid, Buffer *str, int part) {
	BufferPtr *local;

	switch (part) {
		case JID_NODE:
			local = &jid->node;
			break;
		case JID_HOST:
			local = &jid->host;
			break;
		case JID_RESOURCE:
			local = &jid->resource;
			break;
		default:
			return 1;
	}
	if (!str->data) {
		return !local->data;
	}

	if (BPT_SIZE(local) == str->size) {
		return strncasecmp(local->data, str->data, str->size);
	} else {
		return 1;
	}
}

void jid_cpy(Jid *dst, Jid *src, int parts) {
	int size = 0, node_size, host_size, resource_size;
	char *mem = 0;

	// Assume it's nonsence to have a copy of jid w/o hostname

	if (parts & JID_NODE) {
		if ((node_size = BPT_SIZE(&src->node))) {
			size += node_size + 1;
		} else {
			// Source has no node, no reason to copy it
			parts &= ~JID_NODE;
		}
	}
	size += (host_size = BPT_SIZE(&src->host));
	if (parts & JID_RESOURCE) {
		if ((resource_size = BPT_SIZE(&src->resource))) {
			size += resource_size + 1;
		} else {
			// Source has no resource, no reason to copy it
			parts &= ~JID_RESOURCE;
		}
	}

	mem = malloc(size);

	memset(dst, 0, sizeof(*dst));
	if (parts & JID_NODE) {
		dst->node.data = mem;
		memcpy(dst->node.data, src->node.data, node_size);
		dst->node.end = dst->node.data + node_size;
		mem += node_size;
		*(mem++) = '@';
	}
	dst->host.data = mem;
	memcpy(dst->host.data, src->host.data, host_size);
	dst->host.end = dst->host.data + host_size;
	mem += host_size;
	if (parts & JID_RESOURCE) {
		*(mem++) = '/';
		dst->resource.data = mem;
		memcpy(dst->resource.data, src->resource.data, resource_size);
		dst->resource.end = dst->resource.data + resource_size;
	}

	LDEBUG("copied JID with mode %d (%d bytes)\n"
			"from: '%.*s'\n"
			"to:   '%.*s'",
			parts, size,
			JID_LEN(src), JID_STR(src),
			JID_LEN(dst), JID_STR(dst));
}

void jid_destroy(Jid *jid) {
	free(JID_STR(jid));
}

BOOL jid_serialize(Jid *jid, FILE *output) {
	BufferPtr jid_buffer_ptr;
	jid_buffer_ptr.data = JID_STR(jid);
	jid_buffer_ptr.end = jid_buffer_ptr.data + JID_LEN(jid);
	return buffer_ptr_serialize(&jid_buffer_ptr, output);
}

BOOL jid_deserialize(Jid *jid, FILE *input) {
	BufferPtr jid_buffer_ptr;
	if (!buffer_ptr_deserialize(&jid_buffer_ptr, input, MAX_JID_SIZE)) {
		return FALSE;
	}
	return jid_struct(&jid_buffer_ptr, jid);
}
