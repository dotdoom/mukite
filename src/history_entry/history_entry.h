#ifndef HISTORY_ENTRY_H
#define HISTORY_ENTRY_H

#include <time.h>

#define MAX_HISTORY_HEADER_SIZE 65000
#define MAX_HISTORY_INNER_SIZE 65000

#include "xmcomp/src/buffer.h"

typedef struct HistoryEntry {
	BufferPtr nick, header, inner;
	time_t delay;
	struct HistoryEntry *prev, *next;
} HistoryEntry;

HistoryEntry *history_entry_init(HistoryEntry *);
HistoryEntry *history_entry_destroy(HistoryEntry *);

BOOL history_entry_serialize(HistoryEntry *, FILE *);
BOOL history_entry_deserialize(HistoryEntry *, FILE *);

BOOL history_entry_set_header(HistoryEntry *, BufferPtr *);
BOOL history_entry_set_inner(HistoryEntry *, BufferPtr *);
BOOL history_entry_set_nick(HistoryEntry *, BufferPtr *);

#endif
