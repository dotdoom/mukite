#ifndef COMMON_H
#define COMMON_H

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "logger.h"

#ifndef VERSION
#define VERSION -1
#endif

typedef struct {
	char *data;
	int size;
} Buffer;
#define B_EQ_LIT(literal, b) \
	(sizeof(literal)-1 == (b)->size && \
	 !memcmp((literal), (b)->data, sizeof(literal)-1))

typedef struct {
	char *data, *end;
} BufferPtr;
#define BPT_SIZE(bptr) ((int)((bptr)->end - (bptr)->data))
#define BPT_EQ_LIT(literal, bptr) \
	(sizeof(literal)-1 == BPT_SIZE(bptr) && \
	 !memcmp((literal), (bptr)->data, sizeof(literal)-1))

#endif
