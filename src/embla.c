#include <stdlib.h>
#include <time.h>

#include "embla/embla.h"
#include "embla/log.h"
#include "embla/process_manager.h"
#include "embla/scheduler.h"
#include "embla/executor.h"

struct Embla
{
	ProcessManager *process_manager;
	Scheduler *scheduler;
	Executor *executor;

	EmblaState state;
};

Embla *embla_create(void)
{
	Embla *embla = malloc(sizeof(*embla));

	if (embla == NULL)
	{
		embla_log_error("failed to allocate Embla runtime");
		return NULL;
	}

	embla->state = EMBLA_STOPPED;

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

	embla->executor = executor_create();

	if (embla->executor == NULL)
	{
		scheduler_destroy(embla->scheduler);
		process_manager_destroy(embla->process_manager);

		free(embla);

		return NULL;
	}

	embla_log_info("runtime created");

	return embla;
}

static int embla_spawn_init(Embla *embla)
{
	Process *process = process_manager_create_process(embla->process_manager, "init");

	if (process == NULL)
	{
		embla_log_error("failed to create init process");

		return -1;
	}

	char *argv[] = {
		"echo",
		"hello from Embla",
		NULL};

	if (executor_spawn(
			embla->executor,
			process,
			"/bin/echo",
			argv) != 0)
	{
		embla_log_error("failed to spawn init process");

		return -1;
	}

	if (process_transition(process, PROCESS_READY) != 0)
	{
		return -1;
	}

	if (scheduler_add(embla->scheduler, process) != 0)
	{
		return -1;
	}

	return 0;
}

static int embla_shutdown(Embla *embla)
{
	if (embla == NULL)
	{
		return -1;
	}

	if (embla->state != EMBLA_STOPPING)
	{
		return -1;
	}

	embla->state = EMBLA_STOPPED;

	return 0;
}

static int embla_reap_process(Embla *embla, Process *process)
{
	if (embla == NULL || process == NULL)
	{
		return -1;
	}

	if (process_get_state(process) != PROCESS_TERMINATED)
	{
		return -1;
	}

	ProcessId id = process_get_id(process);

	return process_manager_destroy_process(embla->process_manager, id);
}

int embla_run(Embla *embla)
{
	if (embla == NULL)
	{
		return -1;
	}

	if (embla->state != EMBLA_STOPPED)
	{
		return -1;
	}

	embla->state = EMBLA_RUNNING;

	if (embla_spawn_init(embla) != 0)
	{
		embla->state = EMBLA_STOPPED;

		return -1;
	}

	if (scheduler_dispatch(embla->scheduler) != 0)
	{
		embla_log_error("failed to dispatch init process");

		embla->state = EMBLA_STOPPED;

		return -1;
	}

	while (embla->state == EMBLA_RUNNING)
	{
		Process *process = scheduler_current(embla->scheduler);

		if (process == NULL)
		{
			embla_stop(embla);
			break;
		}

		int result = executor_poll(embla->executor, process);

		if (result < 0)
		{
			embla->state = EMBLA_STOPPED;
			return -1;
		}

		if (result == 1)
		{
			if (embla_reap_process(embla, process) != 0)
			{
				embla_log_error("failed to reap process");

				embla->state = EMBLA_STOPPED;

				return -1;
			}

			break;
		}

		struct timespec delay = {
			.tv_sec = 0,
			.tv_nsec = 1000000};

		nanosleep(&delay, NULL);
	}

	if (embla->state == EMBLA_STOPPING)
	{
		if (embla_shutdown(embla) != 0)
		{
			embla_log_error("failed to shut down Embla");
			return -1;
		}
	}

	return 0;
}

int embla_stop(Embla *embla)
{
	if (embla == NULL)
	{
		return -1;
	}

	if (embla->state != EMBLA_RUNNING)
	{
		return 0;
	}

	embla->state = EMBLA_STOPPING;

	return 0;
}

void embla_destroy(Embla *embla)
{
	if (embla == NULL)
	{
		return;
	}

	embla_log_info("runtime shutting down");

	executor_destroy(embla->executor);
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

Executor *embla_executor(Embla *embla)
{
	if (embla == NULL)
	{
		return NULL;
	}

	return embla->executor;
}

EmblaState embla_get_state(const Embla *embla)
{
	if (embla == NULL)
	{
		return EMBLA_STOPPED;
	}

	return embla->state;
}

const char *embla_state_name(EmblaState state)
{
	switch (state)
	{
	case EMBLA_STOPPED:
		return "STOPPED";

	case EMBLA_RUNNING:
		return "RUNNING";

	case EMBLA_STOPPING:
		return "STOPPING";

	default:
		return "UNKNOWN";
	}
}