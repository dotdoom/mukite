#ifndef DLS_LIST_H
#define DLS_LIST_H

#include <string.h>

#include "uthash/src/utlist.h"
#include "uthash/src/uthash.h"
#include "xmcomp/src/logger.h"

#include "serializer.h"

// TODO(artem): make sure these macros are used, not custom overrides.
#define DLS_DECLARE(entry_type) \
	entry_type *head; \
	int size, max_size;

#define DLS_INIT(list, limit) \
	{ \
		memset((list), 0, sizeof(*list)); \
		(list)->max_size = (limit); \
	}

#define DLS_PREPEND(list, element) \
	{ \
		DL_PREPEND((list)->head, element); \
		++(list)->size; \
	}

#define DLS_DELETE(list, element) \
	{ \
		DL_DELETE((list)->head, (element)); \
		--(list)->size; \
	}

// TODO(artem): merge hash/list serializers, if possible.
#define _L_SERIALIZE(iterator, list, element, properties) \
	/* Write a mark that the list exists. */ \
	if (!SERIALIZE_BASE((list)->head)) { \
		LERROR("serializer: cannot write list flag"); \
		return FALSE; \
	} \
	int __serialized_elements__ = 0; \
	iterator((list)->head, element) { \
		if (++__serialized_elements__ > (list)->max_size) { \
			LERROR("serializer: list size %d exceeds maximum size of %d. " \
					"Not serializing the rest to avoid deserialization problems.", \
					(list)->size, (list)->max_size); \
			break; \
		} \
		if (!(properties) || !SERIALIZE_BASE((element)->next) /* Write a mark that the next element exists. */) { \
			LERROR("serializer: cannot write list item"); \
			return FALSE; \
		} \
	}

#define _H_SERIALIZE(hash, element, type, properties) \
	{ \
		void *__presence_mark__ = (void *)1; \
		int __serialized_elements__ = 0; \
		type *__tmp__ = 0; \
		HASH_ITER(hh, (hash)->head, element, __tmp__) { \
			if (++__serialized_elements__ > (hash)->max_size) { \
				LERROR("serializer: hash size %d exceeds maximum size of %d. " \
						"Not serializing the rest to avoid deserialization problems.", \
						(hash)->size, (hash)->max_size); \
				break; \
			} \
			if (!SERIALIZE_BASE(__presence_mark__) || !(properties)) { \
				LERROR("serializer: cannot write list item"); \
				return FALSE; \
			} \
		} \
		__presence_mark__ = 0; \
		/* Write a mark that there are no more elements in the hash. */ \
		if (!SERIALIZE_BASE(__presence_mark__)) { \
			LERROR("serializer: cannot write hash finish mark"); \
			return FALSE; \
		} \
	}

#define DLS_SERIALIZE(list, element, properties) \
	_L_SERIALIZE(DL_FOREACH, list, element, properties)

#define _L_DESERIALIZE(list, element, properties, backref) \
	if (!DESERIALIZE_BASE((list)->head)) { \
		LERROR("deserializer: cannot read list presence mark"); \
		return FALSE; \
	} \
	if (!(list)->head) { \
		LDEBUG("deserializer: the list is empty"); \
	} else { \
		(list)->head = element = 0; \
		(list)->size = 0; \
		do { \
			if (++(list)->size > (list)->max_size) { \
				LERROR("deserializer: list size limit %d exceeded, aborting", (list)->max_size); \
				return FALSE; \
			} \
			if (element) { \
				element->next = malloc(sizeof(*element)); \
				memset(element->next, 0, sizeof(*element)); \
				backref; \
				element = element->next; \
			} else { \
				(list)->head = element = malloc(sizeof(*element)); \
				memset(element, 0, sizeof(*element)); \
			} \
			if (!(properties) || !DESERIALIZE_BASE(element->next)) { \
				LERROR("deserializer: cannot read list item %d", (list)->size); \
				return FALSE; \
			} \
		} while (element->next); \
	}

#define DLS_DESERIALIZE(list, element, properties) \
	_L_DESERIALIZE(list, element, properties, element->next->prev = element)

#define _H_DESERIALIZE(hash, element, key, key_size, properties) \
	{ \
		void *__presence_mark__ = (void *)1; \
		if (!DESERIALIZE_BASE((hash)->head)) { \
			LERROR("deserializer: cannot read hash presence mark"); \
			return FALSE; \
		} \
		if (!(hash)->head) { \
			LDEBUG("deserializer: the hash is empty"); \
		} else { \
			(hash)->head = element = 0; \
			(hash)->size = 0; \
			do { \
				if (++(hash)->size > (hash)->max_size) { \
					LERROR("deserializer: hash size limit %d exceeded, aborting", (hash)->max_size); \
					return FALSE; \
				} \
				if (!(properties) || !DESERIALIZE_BASE(__presence_mark__)) { \
					LERROR("deserializer: cannot read hash item %d", (hash)->size); \
					return FALSE; \
				} \
				HASH_ADD(hh, (hash)->head, key, key_size, element); \
			} while (__presence_mark__); \
		} \
	}

#define DLS_CLEAR(list, destructor, type) \
	{ \
		type *current = 0, *tmp = 0; \
		DL_FOREACH_SAFE((list)->head, current, tmp) { \
			DL_DELETE((list)->head, current); \
			free(destructor(current)); \
		} \
		(list)->size = 0; \
	}

#endif
