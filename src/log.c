#include <stdio.h>

#include "embla/log.h"

static void embla_log(FILE *stream, const char *prefix, const char *message)
{
	fprintf(stream, "%s %s\n", prefix, message != NULL ? message : "(null)");
}

void embla_log_info(const char *message)
{
	embla_log(stdout, "[INFO]", message);
}

void embla_log_error(const char *message)
{
	embla_log(stderr, "[ERROR]", message);
}
