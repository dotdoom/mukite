#ifndef TIMER_H
#define TIMER_H

#include <time.h>

#define TIMER_RESOLUTION 10

void timer_start();
int timer_ticks();
time_t timer_time();

#endif
