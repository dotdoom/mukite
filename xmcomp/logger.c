#include "logger.h"

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

