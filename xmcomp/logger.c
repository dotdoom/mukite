#include "logger.h"

#ifdef LOG_PTHREAD
	pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

#ifdef LOG_CTIME
	time_t log_timestamp;
#endif

int log_level = 0;
const char* log_level_names[] = {
	"debug",
	" info",
	" warn",
	"error"
};

