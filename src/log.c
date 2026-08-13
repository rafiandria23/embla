#include <stdio.h>

#include "embla/log.h"

void embla_log_info(const char *message)
{
	fprintf(stdout, "[INFO] %s\n", message);
}

void embla_log_error(const char *message)
{
	fprintf(stderr, "[ERROR] %s\n", message);
}
