#ifndef SERIALIZER_H
#define SERIALIZER_H

#include <string.h>

#define SERIALIZE_BASE(value) \
	fwrite(&(value), sizeof(value), 1, output)

#define DESERIALIZE_BASE(value) \
	fread(&(value), sizeof(value), 1, input)

#define SERIALIZE_LIST(properties) \
	/* First write a mark that the list exists */ \
	if (!SERIALIZE_BASE(list)) { \
		LERROR("serializer: cannot write list flag"); \
		return FALSE; \
	} \
	for (; list; list = list->next) { \
		if (!(properties) || !SERIALIZE_BASE(list->next)) { \
			LERROR("serializer: cannot write list item"); \
			return FALSE; \
		} \
	} \
	return TRUE;

#define DESERIALIZE_LIST(properties, backref) \
	if (!DESERIALIZE_BASE(*list)) { \
		LERROR("deserializer: cannot read list flag"); \
		return FALSE; \
	} \
	if (!*list) { \
		LDEBUG("deserializer: no items found for the list"); \
		return TRUE; \
	} \
	*list = 0; \
	do { \
		if (++entry_count > limit) { \
			LERROR("deserializer: list size limit exceeded"); \
			return FALSE; \
		} \
		new_entry = malloc(sizeof(*new_entry)); \
		memset(new_entry, 0, sizeof(*new_entry)); \
		if (!(properties) || !DESERIALIZE_BASE(new_entry->next)) { \
			LERROR("deserializer: cannot read list item"); \
			free(new_entry); \
			return FALSE; \
		} \
		if (*list) { \
			(*list)->next = new_entry; \
		} \
		backref; \
		*list = new_entry; \
	} while ((*list)->next); \
	return TRUE;

#endif
