#ifndef XMCOMP_LOGGER_H
#define XMCOMP_LOGGER_H

#include <stdio.h>
#include <time.h>
#include <pthread.h>

#ifdef LOG_PTHREAD
#	define LOG_THREAD_STR "Thread[%lu] "
#	define LOG_THREAD_PAR (unsigned long)pthread_self(),
#else
#	define LOG_THREAD_STR ""
#	define LOG_THREAD_PAR
#endif
#define LOG_THREAD_PRE

#ifdef LOG_CTIME
#	define LOG_TIME_PRE time(&log_timestamp);
#	define LOG_TIME_STR "%s"
#	define LOG_TIME_PAR ctime(&log_timestamp),
	extern time_t log_timestamp;
#else
#	define LOG_TIME_PRE
#	define LOG_TIME_STR ""
#	define LOG_TIME_PAR
#endif

extern int log_level;
extern const char* log_level_names[];

#define LOG_DEBUG 0
#define LOG_INFO  1
#define LOG_WARN  2
#define LOG_ERROR 3

#define LOG(level, format, ...) { \
		if (level >= log_level) { \
			LOG_THREAD_PRE \
			LOG_TIME_PRE \
			fprintf(stderr, "%s (%d)> " LOG_THREAD_STR LOG_TIME_STR "[%s %s] %s:%s():%d: " format "\n", \
					(level >= 0 && level < 4) ? log_level_names[level] : "unknw", \
					level, LOG_THREAD_PAR LOG_TIME_PAR \
					__DATE__, __TIME__, __FILE__, __func__, __LINE__, ## __VA_ARGS__); \
		} \
	}

#define LDEBUG(format, ...) LOG(LOG_DEBUG, format, ## __VA_ARGS__)
#define LINFO(format, ...)  LOG(LOG_INFO,  format, ## __VA_ARGS__)
#define LWARN(format, ...)  LOG(LOG_WARN,  format, ## __VA_ARGS__)
#define LERROR(format, ...) LOG(LOG_ERROR, format, ## __VA_ARGS__)
#define LFATAL(format, ...) { LOG(666, format, ## __VA_ARGS__); exit(1); }
#define LASSERT(expr, msg_format, ...) { if (!(expr)) { LWARN("Assertion failure for %s:\n" msg_format, #expr, ## __VA_ARGS__) } }
#define LERRNO(format, errno, ...) LERROR(format ": %s", ## __VA_ARGS__, strerror(errno))

#endif
