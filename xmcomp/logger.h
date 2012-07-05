#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <time.h>
#include <pthread.h>

extern time_t log_timestamp;
extern int log_level;

extern const char* log_level_names[];

#define LOG_DEBUG 0
#define LOG_INFO  1
#define LOG_WARN  2
#define LOG_ERROR 3

#define LOG(level, format, ...) { \
		if (level >= log_level) { \
			time(&log_timestamp); \
			fprintf(stderr, "%s (%d)> Thread[%lu] %s[%s %s] %s:%s():%d: " format "\n", \
					level < 4 ? log_level_names[level] : "unkwn", \
					level, (unsigned long)pthread_self(), ctime(&log_timestamp), \
					__DATE__, __TIME__, __FILE__, __func__, __LINE__, ## __VA_ARGS__); \
		} \
	}

#define LDEBUG(format, ...) LOG(LOG_DEBUG, format, ## __VA_ARGS__)
#define LINFO(format, ...)  LOG(LOG_INFO,  format, ## __VA_ARGS__)
#define LWARN(format, ...)  LOG(LOG_WARN,  format, ## __VA_ARGS__)
#define LERROR(format, ...) LOG(LOG_ERROR, format, ## __VA_ARGS__)
#define LFATAL(format, ...) { LOG(666, format, ## __VA_ARGS__); exit(1); }
#define LASSERT(expr, msg_format, ...) { if (!(expr)) { LWARN("Assertion failure for %s:\n" msg_format, ## __VA_ARGS__) } }
#define LERRNO(format, errno, ...) LERROR(format ": %s", ## __VA_ARGS__, strerror(errno))

#endif
