#ifndef TIMER_H
#define TIMER_H

#include <pthread.h>

#define TIMER_RESOLUTION 10
#define TIMER_SYNC 600

typedef struct {
	time_t start;
	int ticks;
	pthread_t thread;
} TimerConfig;

void *timer_thread_entry(void *void_timer_config);

#endif
