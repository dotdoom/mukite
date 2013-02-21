#include <string.h>

#include "serializer.h"
#include "jid.h"

#include "history_entry.h"

HistoryEntry *history_entry_init(HistoryEntry *history_entry) {
	return memset(history_entry, 0, sizeof(*history_entry));
}

BOOL history_entry_serialize(HistoryEntry *history_entry, FILE *output) {
	return
		buffer_ptr_serialize(&history_entry->nick, output) &&
		buffer_ptr_serialize(&history_entry->header, output) &&
		buffer_ptr_serialize(&history_entry->inner, output) &&
		SERIALIZE_BASE(history_entry->delay);
}

BOOL history_entry_deserialize(HistoryEntry *history_entry, FILE *input) {
	return
		buffer_ptr_deserialize(&history_entry->nick, input, MAX_JID_PART_SIZE) &&
		buffer_ptr_deserialize(&history_entry->header, input, MAX_HISTORY_HEADER_SIZE) &&
		buffer_ptr_deserialize(&history_entry->inner, input, MAX_HISTORY_INNER_SIZE) &&
		DESERIALIZE_BASE(history_entry->delay);
}

BOOL history_entry_set_header(HistoryEntry *history_entry, BufferPtr *header) {
	BPT_SETTER(history_entry->header, header, MAX_HISTORY_HEADER_SIZE);
}

BOOL history_entry_set_inner(HistoryEntry *history_entry, BufferPtr *inner) {
	BPT_SETTER(history_entry->inner, inner, MAX_HISTORY_INNER_SIZE);
}

BOOL history_entry_set_nick(HistoryEntry *history_entry, BufferPtr *nick) {
	BPT_SETTER(history_entry->nick, nick, MAX_JID_PART_SIZE);
}

HistoryEntry *history_entry_destroy(HistoryEntry *history_entry) {
	free(history_entry->nick.data);
	free(history_entry->header.data);
	free(history_entry->inner.data);
	return history_entry;
}
