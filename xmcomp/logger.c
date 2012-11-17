#include "logger.h"

#ifdef LOG_PTHREAD
	pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

#ifdef LOG_CTIME
	time_t log_timestamp;
#endif

#ifdef LOG_COLOR
	const char* log_level_colors[] = {
		"\e[1;32m", // debug: Bold Green
		"\e[1;36m", // info:  Bold Cyan
		"\e[1;33m", // warn:  Bold Yellow
		"\e[1;31m", // error: Bold Red
		"\e[1;35m"  // unkwn: Bold Purple
	};
#endif

int log_level = 0;
const char* log_level_names[] = {
	"debug",
	"info ",
	"warn ",
	"error"
};

