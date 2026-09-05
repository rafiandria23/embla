#include "embla/platform.h"

int platform_memory_limit_supported(void)
{
	return 0;
}

int platform_apply_memory_limit(rlim_t bytes)
{
	(void)bytes;

	return -1;
}
