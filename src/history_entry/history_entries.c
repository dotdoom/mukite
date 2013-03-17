#include "xmcomp/src/logger.h"

#include "ut2s.h"
#include "builder.h"
#include "timer.h"
#include "history_entry/history_entry.h"

#include "history_entries.h"

BOOL history_entries_serialize(HistoryEntriesList *history_entries, FILE *output) {
	HistoryEntry *current = 0;
	DLS_SERIALIZE(history_entries, current, history_entry_serialize(current, output));
	return TRUE;
}

BOOL history_entries_deserialize(HistoryEntriesList *history_entries, FILE *input) {
	HistoryEntry *current = 0;
	DLS_DESERIALIZE(history_entries, current, history_entry_deserialize(current, input));
	return TRUE;
}

HistoryEntry *history_entries_push(HistoryEntriesList *history_entries) {
	// TODO(artem): remove history items if the size exceeds max_size (max_size has been decreased).

	if (history_entries->max_size <= 0) { 
		return 0;
	}

	HistoryEntry *history_entry = 0;
	if (history_entries->size < history_entries->max_size) {
		history_entry = history_entry_init(malloc(sizeof(*history_entry)));
		DLS_APPEND(history_entries, history_entry);
		++history_entries->size;
	} else {
		history_entry = history_entries->head;
		DLS_DELETE(history_entries, history_entry);
		DLS_APPEND(history_entries, history_entry);
	}

	return history_entry;
}

/*HistoryEntry *history_entries_head_for_limit(HistoryEntriesList *history_entries, HistoryLimit *limit) {
	if (limit->max_chars < 0 && limit->max_stanzas < 0 && limit->seconds < 0 ||
			!history_entries->head) { 
		// Shortcut.
		return history->head;
	}

	time_t now = timer_time();
	HistoryEntry *current = 0;
	DLS_FOREACH(history_entries->head, current) {
		if (
				(limit->max_chars >= 0 &&
				 (limit->max_chars -= BPT_SIZE(&current->inner)) < 0) ||
				(limit->max_stanzas >= 0 &&
				 !limit->max_stanzas--) ||
				(limit->seconds >= 0 &&
				 limit->seconds < difftime(now, current->delay))
		   ) {
			return current == history_entries->head ? 0 : current->prev;
		}
	}
	return history_entries->head;
}*/

/*void history_send(Buffer *node, HistoryEntry *history, ParticipantEntry *receiver) {
	BuilderPacket egress = {};

	egress.name = STANZA_MESSAGE;
	egress.type = STANZA_MESSAGE_GROUPCHAT;
	egress.from_node = *node;
	egress.to = receiver->jid;

	for (; history; history = history->next) {
		egress.from_nick = history->nick;
		egress.header = history->header;
		egress.user_data = history->inner;
		egress.delay = history->delay;
		worker_send(&egress);
	}
}*/
