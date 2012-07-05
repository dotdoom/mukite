#include "logger.h"

time_t log_timestamp;
int log_level = 0;
const char* log_level_names[] = {
	"debug",
	" info",
	" warn",
	"error"
};

