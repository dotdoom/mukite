#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "buffer.h"
#include "logger.h"

BOOL buffer_serialize(Buffer *buffer, FILE *output) {
	return fwrite(&buffer->size, sizeof(buffer->size), 1, output) &&
		(!buffer->size || fwrite(buffer->data, buffer->size, 1, output));
}

BOOL buffer_deserialize(Buffer *buffer, FILE *input, int limit) {
	int error;
	if (!fread(&buffer->size, sizeof(buffer->size), 1, input)) {
		error = errno;
		LERRNO("fread() failed", error);
		return FALSE;
	}
	if (buffer->size < 0 || buffer->size > limit) {
		LERROR("buffer size = %d (limit %d)", buffer->size, limit);
		return FALSE;
	}
	if (!buffer->size) {
		buffer->data = 0;
		return TRUE;
	}
	buffer->data = malloc(buffer->size);
	return fread(buffer->data, buffer->size, 1, input);
}

BOOL buffer_ptr_serialize(BufferPtr *buffer_ptr, FILE *output) {
	Buffer buffer = BPT_2_BUF(buffer_ptr);
	return buffer_serialize(&buffer, output);
}

BOOL buffer_ptr_deserialize(BufferPtr *buffer_ptr, FILE *input, int limit) {
	Buffer buffer;
	if (!buffer_deserialize(&buffer, input, limit)) {
		return FALSE;
	}
	buffer_ptr->data = buffer.data;
	buffer_ptr->end = buffer.data + buffer.size;
	return TRUE;
}

void buffer_ptr_cpy(BufferPtr *to, BufferPtr *from) {
	int size = BPT_SIZE(from);
	to->data = malloc(size);
	to->end = to->data + size;
	memcpy(to->data, from->data, size);
}

void buffer_ptr__cpy(BufferPtr *to, Buffer *from) {
	to->data = malloc(from->size);
	to->end = to->data + from->size;
	memcpy(to->data, from->data, from->size);
}

void buffer__ptr_cpy(Buffer *to, BufferPtr *from) {
	to->size = BPT_SIZE(from);
	to->data = malloc(to->size);
	memcpy(to->data, from->data, to->size);
}

int btoi(BufferPtr *bptr) {
	char int_buffer[21];
	int size = BPT_SIZE(bptr);
	if (size >= sizeof(int_buffer)) {
		return 0;
	} else {
		memcpy(int_buffer, bptr->data, size);
		int_buffer[size] = 0;
		return atoi(int_buffer);
	}
}
