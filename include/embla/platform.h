#ifndef EMBLA_PLATFORM_H
#define EMBLA_PLATFORM_H

#include <sys/resource.h>

int platform_apply_rlimit(
	int resource,
	rlim_t requested);

int platform_memory_limit_supported(void);

int platform_apply_memory_limit(rlim_t bytes);

#endif
