#ifndef TIMER_H
#define TIMER_H

#define TIMER_RESOLUTION 10

void timer_start();
inline int timer_ticks();
inline time_t timer_time();

#endif
