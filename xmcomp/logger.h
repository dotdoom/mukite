#ifndef XMCOMP_LOGGER_H
#define XMCOMP_LOGGER_H

#include <stdio.h>

#ifdef LOG_POS
#	define LOG_POS_PRE
#	define LOG_POS_STR "[%s %s] %s:%s():%d: "
#	define LOG_POS_PAR ,__DATE__, __TIME__, __FILE__, __func__, __LINE__
#	define LOG_POS_POST
#else
#	define LOG_POS_PRE
#	define LOG_POS_POST
#	define LOG_POS_STR
#	define LOG_POS_PAR
#endif

#ifdef LOG_CTIME
#	include <time.h>
	extern time_t log_timestamp;
#	define LOG_TIME_PRE time(&log_timestamp);
#	define LOG_TIME_STR "%s"
#	define LOG_TIME_PAR ,ctime(&log_timestamp)
#	define LOG_TIME_POST
#else
#	define LOG_TIME_PRE
#	define LOG_TIME_POST
#	define LOG_TIME_STR
#	define LOG_TIME_PAR
#endif

#ifdef LOG_PTHREAD
#	include <pthread.h>
	extern pthread_mutex_t log_mutex;
#	define LOG_THREAD_PRE pthread_mutex_lock(&log_mutex);
#	define LOG_THREAD_STR "Thread[%lu] "
#	define LOG_THREAD_PAR ,(unsigned long)pthread_self()
#	define LOG_THREAD_POST pthread_mutex_unlock(&log_mutex);
#else
#	define LOG_THREAD_PRE
#	define LOG_THREAD_STR
#	define LOG_THREAD_PAR
#	define LOG_THREAD_POST
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
			fprintf(stderr, "%s (%d)> " LOG_THREAD_STR LOG_TIME_STR LOG_POS_STR format "\n", \
					(level >= 0 && level < 4) ? log_level_names[level] : "unkwn", \
					level LOG_THREAD_PAR LOG_TIME_PAR LOG_POS_PAR, ## __VA_ARGS__); \
			LOG_TIME_POST \
			LOG_THREAD_POST \
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
