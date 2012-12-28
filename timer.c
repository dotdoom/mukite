#include <time.h>

#include "xmcomp/sighelper.h"

#include "timer.h"

void *timer_thread_entry(void *void_timer_config) {
	TimerConfig *config = void_timer_config;
	struct timespec rqtp = {
		.tv_sec = 0,
		.tv_nsec = 1000000000 / TIMER_RESOLUTION
	};
	time_t pivot_time;
	int i;

	sighelper_sigblockall(0);
	time(&config->start);
	config->ticks = 0;

	while (1) {
		time(&pivot_time);
		config->ticks = difftime(pivot_time, config->start) * 10;
		for (i = 0; i < TIMER_SYNC; ++i) {
			nanosleep(&rqtp, 0);
			++config->ticks;
		}
	}
}
