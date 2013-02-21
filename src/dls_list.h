#ifndef DLS_LIST_H
#define DLS_LIST_H

#include <string.h>

#include "uthash/src/utlist.h"

#include "serializer.h"

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

#define _L_SERIALIZE(iterator, list, element, properties) \
	/* Write a mark that the list exists. */ \
	if (!SERIALIZE_BASE((list)->head)) { \
		LERROR("serializer: cannot write list flag"); \
		return FALSE; \
	} \
	int _serialized_elements = 0; \
	iterator((list)->head, element) \
		if (++_serialized_elements > (list)->max_size) { \
			LERROR("serializer: list size %d exceeds maximum size of %d. " \
					"Not serializing the rest to avoid deserialization problems.", \
					(list)->size, (list)->max_size); \
			break; \
		} \
		if (!(properties) || !SERIALIZE_BASE((element)->next)) { \
			LERROR("serializer: cannot write list item"); \
		}

#define DLS_SERIALIZE(list, element, properties) \
	_L_SERIALIZE(DL_FOREACH, list, element, properties)

#define _L_DESERIALIZE(list, element, properties, backref) \
	if (!DESERIALIZE_BASE((list)->head)) { \
		LERROR("deserializer: cannot read list flag"); \
		return FALSE; \
	} \
	if (!(list->head)) { \
		LDEBUG("deserializer: the list is empty"); \
		return TRUE; \
	} \
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
	} while (element->next);

#define DLS_DESERIALIZE(list, element, properties) \
	_L_DESERIALIZE(list, element, properties, element->next->prev = element)

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
