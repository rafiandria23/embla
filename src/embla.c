#include <stdlib.h>

#include "embla/embla.h"
#include "embla/log.h"
#include "embla/process_manager.h"
#include "embla/scheduler.h"

struct Embla
{
	int running;

	ProcessManager *process_manager;
	Scheduler *scheduler;
};

Embla *embla_create(void)
{
	Embla *embla = malloc(sizeof(*embla));

	if (embla == NULL)
	{
		embla_log_error("failed to allocate Embla runtime");
		return NULL;
	}

	embla->running = 0;

	embla->process_manager = process_manager_create();

	if (embla->process_manager == NULL)
	{
		embla_log_error("failed to create process manager");

		free(embla);

		return NULL;
	}

	embla->scheduler = scheduler_create();

	if (embla->scheduler == NULL)
	{
		embla_log_error("failed to create scheduler");
		process_manager_destroy(embla->process_manager);

		free(embla);

		return NULL;
	}

	embla_log_info("runtime created");

	return embla;
}

int embla_run(Embla *embla)
{
	if (embla == NULL)
	{
		return -1;
	}

	embla->running = 1;

	embla_log_info("runtime started");

	return 0;
}

void embla_destroy(Embla *embla)
{
	if (embla == NULL)
	{
		return;
	}

	embla_log_info("runtime shutting down");

	scheduler_destroy(embla->scheduler);
	process_manager_destroy(embla->process_manager);

	free(embla);
}

ProcessManager *embla_process_manager(Embla *embla)
{
	if (embla == NULL)
	{
		return NULL;
	}

	return embla->process_manager;
}

Scheduler *embla_scheduler(Embla *embla)
{
	if (embla == NULL)
	{
		return NULL;
	}

	return embla->scheduler;
}
