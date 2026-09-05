#define _POSIX_C_SOURCE 200809L

#include "embla/platform.h"

int platform_memory_limit_supported(void)
{
	return 1;
}

int platform_apply_memory_limit(rlim_t bytes)
{
	return platform_apply_rlimit(RLIMIT_AS, bytes);
}
