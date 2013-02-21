#ifndef BUFFER_H
#define BUFFER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "logger.h"

typedef struct {
	char *data;
	int size;
} Buffer;
#define BUF_INIT(b) { \
		(b)->data = 0; \
		(b)->size = 0; \
	}
#define BUF_END(b) ((b)->data + (b)->size)
#define BUF_NULL(b) (!(b)->data)
#define BUF_EMPTY(b) (!(b)->size)
#define BUF_BLANK(b) (BUF_NULL(b) || BUF_EMPTY(b))
#define BUF_EQ_LIT(literal, b) \
	(sizeof(literal)-1 == (b)->size && \
	 !memcmp((literal), (b)->data, sizeof(literal)-1))
#define BUF_EQ_BIN(bin, b, len) \
	((b)->size == (len) && !memcmp((bin), (b)->data, (len)))

BOOL buffer_serialize(Buffer *, FILE *);
BOOL buffer_deserialize(Buffer *, FILE *, int);

typedef struct {
	char *data, *end;
} BufferPtr;
#define BPT_INIT(bptr) { \
		(bptr)->data = 0; \
		(bptr)->end = 0; \
	}
#define BPT_SIZE(bptr) ((int)((bptr)->end - (bptr)->data))
#define BPT_NULL(bptr) (!(bptr)->data)
#define BPT_EMPTY(bptr) (!BPT_SIZE(bptr))
#define BPT_BLANK(bptr) (BPT_NULL(bptr) || BPT_EMPTY(bptr))
#define BPT_EQ_LIT(literal, bptr) \
	(sizeof(literal)-1 == BPT_SIZE(bptr) && \
	 !memcmp((literal), (bptr)->data, sizeof(literal)-1))
#define BPT_SET_LIT(bptr, literal) \
	{ (bptr)->data = literal; (bptr)->end = (bptr)->data + sizeof(literal) - 1; }
#define BPT_2_BUF(bptr) \
	{ .data = (bptr)->data, .size = BPT_SIZE(bptr) }
#define BPT_EQ_BIN(bin, bptr, len) \
	(BPT_SIZE(bptr) == (len) && !memcmp((bin), (bptr)->data, (len)))

#define BPT_SETTER(target, new_buffer, limit) \
	{ \
		if (!(new_buffer) || BPT_NULL(new_buffer)) { \
			free((target).data); \
			BPT_INIT(&(target)); \
			return TRUE; \
		} else { \
			int _new_size = BPT_SIZE(new_buffer); \
			if (_new_size > (limit)) { \
				LERROR("trying to set " #target " size %d, which exceeds %d", \
						_new_size, (limit)); \
				LDEBUG(#new_buffer " data: '%.*s'", _new_size, (new_buffer)->data); \
				return FALSE; \
			} \
			(target).end = memcpy((target).data = realloc((target).data, _new_size), (new_buffer)->data, _new_size) + _new_size; \
			return TRUE; \
		} \
	}

BOOL buffer_ptr_serialize(BufferPtr *, FILE *);
BOOL buffer_ptr_deserialize(BufferPtr *, FILE *, int);
void buffer_ptr_cpy(BufferPtr *to, BufferPtr *from);
void buffer_ptr__cpy(BufferPtr *to, Buffer *from);
void buffer__ptr_cpy(Buffer *to, BufferPtr *from);

int btoi(BufferPtr *);

#endif
