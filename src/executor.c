#include <stdlib.h>

#include "embla/log.h"
#include "embla/executor.h"

struct Executor
{
	int placeholder;
};

Executor *executor_create(void)
{
	Executor *executor = malloc(sizeof(*executor));

	if (executor == NULL)
	{
		embla_log_error("failed to allocate executor");

		return NULL;
	}

	executor->placeholder = 0;

	return executor;
}

void executor_destroy(Executor *executor)
{
	if (executor == NULL)
	{
		return;
	}

	free(executor);
}
