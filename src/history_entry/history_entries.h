#ifndef HISTORY_ENTRIES_H
#define HISTORY_ENTRIES_H

#include "history_entry.h"

typedef struct {
	DLS_DECLARE(HistoryEntry);
} HistoryEntriesList;

typedef struct {
	int max_stanzas,
		max_chars,
		seconds;
} HistoryLimit;

BOOL history_entries_serialize(HistoryEntriesList *, FILE *);
BOOL history_entries_deserialize(HistoryEntriesList *, FILE *);

HistoryEntry *history_entries_push(HistoryEntriesList *);

#endif
