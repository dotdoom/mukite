#include <stdlib.h>

#include "buffer.h"

BOOL buffer_serialize(Buffer *buffer, FILE *output) {
	return fwrite(&buffer->size, sizeof(buffer->size), 1, output) &&
		(!buffer->size || fwrite(buffer->data, buffer->size, 1, output));
}

BOOL buffer_deserialize(Buffer *buffer, FILE *input, int limit) {
	if (!fread(&buffer->size, sizeof(buffer->size), 1, input)) {
		return FALSE;
	}
	if (buffer->size < 0 || buffer->size > limit) {
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
