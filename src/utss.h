#ifndef UT2S_H
#define UT2S_H

/*
 * utss: utlist, uthash + size & serialization
 */

#include <string.h>

#include "uthash/src/utlist.h"
#include "uthash/src/uthash.h"
#include "xmcomp/src/logger.h"

#include "serializer.h"

#define DLS_DECLARE(entry_type) \
	entry_type *head; \
	int size, max_size;

#define DLS_INIT(list, limit) \
	{ \
		memset((list), 0, sizeof(*list)); \
		(list)->max_size = (limit); \
	}

#define _DLS_EXPAND_WRAPPER(original, list, element) \
	{ \
		original((list)->head, element); \
		++(list)->size; \
	}

#define DLS_PREPEND(list, element) _DLS_EXPAND_WRAPPER(DL_PREPEND, list, element)
#define DLS_APPEND(list, element) _DLS_EXPAND_WRAPPER(DL_APPEND, list, element)

#define DLS_DELETE(list, element) \
	{ \
		DL_DELETE((list)->head, (element)); \
		--(list)->size; \
	}

#define DLS_CLEAR(list, destructor, type) \
	{ \
		type *current = 0, *tmp = 0; \
		DL_FOREACH_SAFE((list)->head, current, tmp) { \
			DLS_DELETE(list, current); \
			free(destructor(current)); \
		} \
	}

#define DLS_FOREACH(list, element) DL_FOREACH((list)->head, element)

#define _CONTAINER_SERIALIZE(container, iterator, type, entry_serializer) \
	{ \
		LDEBUG("serializing Container<" #type ">."); \
		type *__current__ = 0; \
		void *__presence_mark__ = (void*)1; \
		int __serialized_elements__ = 0; \
		iterator { \
			if (++__serialized_elements__ > (container)->max_size) { \
				LERROR("serializer: container size %d exceeds maximum size of %d. " \
						"Not serializing the rest to avoid deserialization problems.", \
						(container)->size, (container)->max_size); \
				break; \
			} \
			if (!SERIALIZE_BASE(__presence_mark__) || \
					!(entry_serializer(__current__, output))) { \
				LERROR("serializer: cannot write container item."); \
				return FALSE; \
			} \
		} \
		__presence_mark__ = 0; \
		if (!SERIALIZE_BASE(__presence_mark__)) { \
			LERROR("serializer: cannot write container finish mark."); \
			return FALSE; \
		} \
	}

#define DLS_SERIALIZE(list, type, entry_serializer) \
	_CONTAINER_SERIALIZE(list, DLS_FOREACH(list, __current__), type, entry_serializer)

#define HASHS_SERIALIZE(hash, type, entry_serializer) \
	_CONTAINER_SERIALIZE(hash, type *__tmp__ = 0; HASHS_ITER(hash, __current__, __tmp__), type, entry_serializer)

#define _CONTAINER_DESERIALIZE(container, setter, type, entry_deserializer) \
	{ \
		LDEBUG("deserializer: starting Container<" #type ">."); \
		void *__presence_mark__ = 0; \
		if (!DESERIALIZE_BASE(__presence_mark__)) { \
			LERROR("deserializer: cannot read container presence mark."); \
			return FALSE; \
		} \
		if (!__presence_mark__) { \
			LDEBUG("deserializer: the container is empty."); \
		} else { \
			type *__current__ = 0; \
			(container)->head = 0; \
			(container)->size = 0; \
			do { \
				/* TODO(artem): max_size*2 is a hack around failed serializations */ \
				if (++(container)->size > (container)->max_size*2) { \
					LERROR("deserializer: container size limit %d exceeded, aborting.", (container)->max_size); \
					return FALSE; \
				} \
				__current__ = memset(malloc(sizeof(*__current__)), 0, sizeof(*__current__)); \
				if (!(entry_deserializer(__current__, input)) || !DESERIALIZE_BASE(__presence_mark__)) { \
					LERROR("deserializer: cannot read container item %d.", (container)->size); \
					return FALSE; \
				} \
				setter; \
			} while (__presence_mark__); \
			LDEBUG("deserializer: finished Container<" #type ">, total %d items.", (container)->size); \
		} \
	}

#define DLS_DESERIALIZE(list, type, entry_deserializer) \
	_CONTAINER_DESERIALIZE(list, DLS_APPEND((list), __current__), type, entry_deserializer)

#define HASHS_DESERIALIZE(hash, type, key, key_size, entry_deserializer) \
	_CONTAINER_DESERIALIZE(hash, HASHS_ADD_KEYPTR((hash), __current__->key, __current__->key_size, __current__), type, entry_deserializer)

#define HASHS_ITER(hash, element, tmp) HASH_ITER(hh, (hash)->head, element, tmp)

#define HASHS_ADD_KEYPTR(hash, key, key_size, element) \
	{ \
		HASH_ADD_KEYPTR(hh, (hash)->head, key, key_size, element); \
		++(hash)->size; \
	}

#define HASHS_DEL(hash, element) \
	{ \
		HASH_DEL((hash)->head, element); \
		--(hash)->size; \
	}

#define HASHS_FIND(hash, key, size, element) HASH_FIND(hh, (hash)->head, key, size, element)

#endif
