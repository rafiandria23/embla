#define _POSIX_C_SOURCE 200809L

#include "embla/platform.h"

int platform_apply_rlimit(
	int resource,
	rlim_t requested)
{
	struct rlimit current;

	if (getrlimit(resource, &current) != 0)
	{
		return -1;
	}

	rlim_t effective = requested;

	if (
		current.rlim_max != RLIM_INFINITY &&
		effective > current.rlim_max)
	{
		effective = current.rlim_max;
	}

	struct rlimit limit = {
		.rlim_cur = effective,
		.rlim_max = effective};

	return setrlimit(resource, &limit);
}
