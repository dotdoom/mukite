#include <string.h>

#include "xmcomp/logger.h"

#include "mewcate.h"

/*static int count_different_bytes(BufferPtr *buffer) {
	BOOL bytes[256];
	char *current;
	int i, total = 0;
	memset(bytes, 0, sizeof(bytes));
	for (current = buffer->data; current < buffer->end; ++current) {
		bytes[(unsigned char)(*current)] = TRUE;
	}
	for (i = 0; i < 256; ++i) {
		if (bytes[i]) {
			++total;
		}
	}
	LDEBUG("total %d chars", total);
	return total;
}*/

static void byte2_swap(char *b1, char *b2) {
	char tmp = *b1;
	*b1 = *b2;
	*b2 = tmp;

	++b1;
	++b2;

	tmp = *b1;
	*b1 = *b2;
	*b2 = tmp;
}

#define INC_UTF8(value, length) \
	if (((value) & 0xE0) == 0xC0) { \
		length += 2; \
	} else if (((value) & 0xF0) == 0xE0) { \
		length += 3; \
	} else if (((value) & 0xF8) == 0xF0) { \
		length += 4; \
	} else if (((value) & 0xFC) == 0xF8) { \
		length += 5; \
	} else if (((value) & 0xFE) == 0xFC) { \
		length += 6; \
	} else { \
		length += 1; \
	}

static void utf8_random_swap(BufferPtr *data) {
	int swap_index, str_index, str_index_next;
	char *first_swap_index = 0;
	for (swap_index = 0; swap_index < BPT_SIZE(data) / 4; ++swap_index) {
		for (str_index = 0; str_index < BPT_SIZE(data); ) {
			str_index_next = str_index;
			INC_UTF8(*(data->data + str_index), str_index_next);
			if (str_index_next - str_index == 2 && rand() < RAND_MAX / 50) {
				if (first_swap_index) {
					byte2_swap(first_swap_index, data->data + str_index);
				} else {
					first_swap_index = data->data + str_index;
				}
			}
			str_index = str_index_next;
		}
	}
}

static BOOL adhoc_filter(BufferPtr *data) {
	BufferPtr inner = *data, body;
	Buffer node_name;
	if (rand() > RAND_MAX / 3) {
		return TRUE;
	}
	if (xmlfsm_skipto_node(&inner, "body", &body)) {
		xmlfsm_node_name(&body, &node_name);
		if (xmlfsm_skip_attrs(&body)) {
			body.end -= sizeof("</body>") - 1;
			utf8_random_swap(&body);
			/*if (BPT_SIZE(&body) > 10 &&
					count_different_bytes(&body) < 6) {
				BPT_SET_LIT(data, "<body>Happy New Year!</body>");
				return TRUE;
			}*/
		}
	}
	return TRUE;
}

BOOL mewcate_handle(Room *room,
		ParticipantEntry *sender, ParticipantEntry *receiver,
		BuilderPacket *egress, SendCallback *send) {
	return (egress->name != 'm') ||
		adhoc_filter(&egress->user_data);
}
