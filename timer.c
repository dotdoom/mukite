#include <time.h>
#include <pthread.h>

#include "xmcomp/sighelper.h"

#include "timer.h"

#define TIMER_SYNC 600

struct {
	time_t start;
	int ticks;
	pthread_t thread;
} timer_config;

static void *timer_thread_entry(void *_unused) {
	struct timespec rqtp = {
		.tv_sec = 0,
		.tv_nsec = 1000000000 / TIMER_RESOLUTION
	};
	time_t pivot_time;
	int i;

	sighelper_sigblockall(0);
	time(&timer_config.start);
	timer_config.ticks = 0;

	while (1) {
		time(&pivot_time);
		timer_config.ticks = difftime(pivot_time, timer_config.start) * TIMER_RESOLUTION;
		for (i = 0; i < TIMER_SYNC; ++i) {
			nanosleep(&rqtp, 0);
			++timer_config.ticks;
		}
	}

	return 0;
}

void timer_start() {
	pthread_create(&timer_config.thread, 0, timer_thread_entry, 0);
}

inline time_t timer_time() {
	return timer_config.start + timer_config.ticks / TIMER_RESOLUTION;
}

inline int timer_ticks() {
	return timer_config.ticks;
}
