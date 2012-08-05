#include <stdlib.h>
#include <string.h>

#include "xmcomp/logger.h"

#include "jid.h"

BOOL jid_struct(BufferPtr *jid_string, Jid *jid_struct) {
	int part = JID_NODE;
	char *part_start = jid_string->data,
		*current = jid_string->data;

	memset(jid_struct, 0, sizeof(*jid_struct));
	jid_struct->resource.end = jid_string->end;
	for (; current < jid_string->end; ++current) {
		switch (part) {
			case JID_NODE:
				if (*current == '@') {
					jid_struct->node.data = part_start;
					jid_struct->node.end = current;
					part = JID_HOST;
					part_start = current+1;
					break;
				}
				// fallthrough
			case JID_HOST:
				if (*current == '/') {
					jid_struct->host.data = part_start;
					jid_struct->host.end = current;
					part = JID_RESOURCE;
					part_start = current+1;
				}
				break;
			case JID_RESOURCE:
				break;
		}
	}

	switch (part) {
		case JID_NODE:
		case JID_HOST:
			jid_struct->host.data = part_start;
			jid_struct->host.end = current;
			break;
		case JID_RESOURCE:
			jid_struct->resource.data = part_start;
			break;
	}

	if (!jid_struct->host.data ||
			(BPT_SIZE(&jid_struct->host) > JID_PART_LIMIT) ||
			(BPT_SIZE(&jid_struct->node) > JID_PART_LIMIT) ||
			(jid_struct->resource.data && BPT_SIZE(&jid_struct->resource) > JID_PART_LIMIT)) {
		return FALSE;
	}

	LDEBUG("split the JID '%.*s': node '%.*s', host '%.*s', resource '%.*s'",
			JID_LEN(jid_struct), JID_STR(jid_struct),
			jid_struct->node.data ? BPT_SIZE(&jid_struct->node) : 0, jid_struct->node.data,
			BPT_SIZE(&jid_struct->host), jid_struct->host.data,
			jid_struct->resource.data ? BPT_SIZE(&jid_struct->resource) : 0, jid_struct->resource.data);

	return TRUE;
}

int jid_cmp(Jid *jid1, Jid *jid2, int mode) {
	int size;

	LDEBUG("comparing JIDs: '%.*s' <=> '%.*s', mode %d",
			JID_LEN(jid1), JID_STR(jid1),
			JID_LEN(jid2), JID_STR(jid2),
			mode);

	if ((mode & JID_NODE) && (jid1->node.data || !(mode & JID_CMP_NULLWC))) {
		if (!jid1->node.data || !jid2->node.data) {
			LDEBUG("node NULLWC trial");
			return jid1->node.data != jid2->node.data;
		}

		if ((size = BPT_SIZE(&jid1->node)) != BPT_SIZE(&jid2->node)) {
			LDEBUG("node sizes differ");
			return 1;
		}

		if (memcmp(jid1->node.data, jid2->node.data, size)) {
			LDEBUG("nodes differ");
			return 1;
		}
	}

	if (mode & JID_HOST) {
		if ((size = BPT_SIZE(&jid1->host)) != BPT_SIZE(&jid2->host)) {
			LDEBUG("host sizes differ");
			return 1;
		}

		if (memcmp(jid1->host.data, jid2->host.data, size)) {
			LDEBUG("hosts differ: '%.*s' and '%.*s'",
					size, jid1->host.data,
					size, jid2->host.data);
			return 1;
		}
	}

	if ((mode & JID_RESOURCE) && (jid1->resource.data || !(mode & JID_CMP_NULLWC))) {
		if (!jid1->resource.data || !jid2->resource.data) {
			LDEBUG("resources differ");
			return jid1->resource.data != jid2->resource.data;
		}

		if ((size = BPT_SIZE(&jid1->resource)) != BPT_SIZE(&jid2->resource)) {
			LDEBUG("resource sizes differ");
			return 1;
		}

		if (memcmp(jid1->resource.data, jid2->resource.data, size)) {
			LDEBUG("resources differ");
			return 1;
		}
	}

	LDEBUG("JIDs are equal");

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
		return memcmp(local->data, str->data, str->size);
	} else {
		return 1;
	}
}

void jid_cpy(Jid *dst, Jid *src) {
	// XXX: this should check each part

	int size = src->resource.end - src->node.data;
	dst->node.data = malloc(size);
	dst->resource.end = dst->node.data + size;
	memcpy(dst->node.data, src->node.data, size);

	dst->node.end = dst->node.data + BPT_SIZE(&src->node);
	dst->host.data = dst->node.end + 1;
	dst->host.end = dst->host.data + BPT_SIZE(&src->host);
	dst->resource.data = dst->host.end + 1;

	LDEBUG("copied JID (%d bytes)\n"
			"from: '%.*s'\n"
			"to:   '%.*s'",
			size,
			JID_LEN(src), JID_STR(src),
			JID_LEN(dst), JID_STR(dst));
}

void jid_free(Jid *jid) {
	free(JID_STR(jid));
}
