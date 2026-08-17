#include <signal.h>
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

static int embla_handle_child_exit(
	Embla *embla,
	HostProcessId host_id,
	int wait_status)
{
	if (embla == NULL)
	{
		return -1;
	}

	Process *process = process_manager_get_by_host_id(
		embla->process_manager,
		host_id);

	if (process == NULL)
	{
		embla_log_error("received exit status for unknown process");
		return -1;
	}

	if (WIFEXITED(wait_status))
	{
		if (process_set_exit_code(
				process,
				WEXITSTATUS(wait_status)) != 0)
		{
			embla_log_error("failed to set process exit code");
			return -1;
		}
	}
	else if (WIFSIGNALED(wait_status))
	{
		if (process_set_term_signal(
				process,
				WTERMSIG(wait_status)) != 0)
		{
			embla_log_error("failed to set process termination signal");
			return -1;
		}
	}
	else
	{
		return 0;
	}

	if (process_transition(
			process,
			PROCESS_TERMINATED) != 0)
	{
		embla_log_error("failed to transition process to TERMINATED");
		return -1;
	}

	if (process_manager_reparent_children(
			embla->process_manager,
			process_get_id(process),
			EMBLA_ROOT_PID) != 0)
	{
		embla_log_error("failed to reparent orphaned children");
		return -1;
	}

	if (scheduler_remove(
			embla->scheduler,
			process) != 0)
	{
		embla_log_error("failed to remove process from scheduler");
		return -1;
	}

	return 0;
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

	if (scheduler_ready_count(embla->scheduler) > 0)
	{
		if (scheduler_dispatch(embla->scheduler) != 0)
		{
			embla_log_error("failed to dispatch initial process");
			embla->state = EMBLA_STOPPED;
			return -1;
		}
	}

	while (embla->state == EMBLA_RUNNING)
	{
		HostProcessId host_id;
		int wait_status;

		int result = executor_poll_any(
			embla->executor,
			&host_id,
			&wait_status);

		if (result < 0)
		{
			embla_log_error("failed to poll child processes");
			embla->state = EMBLA_STOPPED;
			return -1;
		}

		if (result == 1)
		{
			if (embla_handle_child_exit(
					embla,
					host_id,
					wait_status) != 0)
			{
				embla_log_error("failed to handle child process exit");
				embla->state = EMBLA_STOPPED;
				return -1;
			}

			if (
				scheduler_current(embla->scheduler) == NULL &&
				scheduler_ready_count(embla->scheduler) > 0)
			{
				if (scheduler_dispatch(embla->scheduler) != 0)
				{
					embla_log_error("failed to dispatch next process");
					embla->state = EMBLA_STOPPED;
					return -1;
				}
			}
		}

		if (process_manager_live_count(
				embla->process_manager) == 0)
		{
			embla_stop(embla);
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

static Process *embla_spawn_internal(
	Embla *embla,
	ProcessId parent_id,
	const char *name,
	const char *path,
	char *const argv[])
{
	if (
		embla == NULL ||
		name == NULL ||
		path == NULL ||
		argv == NULL)
	{
		return NULL;
	}

	Process *process = process_manager_create_process(
		embla->process_manager,
		parent_id,
		name);

	if (process == NULL)
	{
		embla_log_error("failed to create process");
		return NULL;
	}

	ProcessId process_id = process_get_id(process);

	if (executor_spawn(
			embla->executor,
			process,
			path,
			argv) != 0)
	{
		embla_log_error("failed to spawn host process");
		process_manager_destroy_process(
			embla->process_manager,
			process_id);
		return NULL;
	}

	if (process_transition(
			process,
			PROCESS_READY) != 0)
	{
		embla_log_error("failed to transition process to READY");

		executor_terminate(
			embla->executor,
			process);
		process_manager_destroy_process(
			embla->process_manager,
			process_id);

		return NULL;
	}

	if (scheduler_add(
			embla->scheduler,
			process) != 0)
	{
		embla_log_error("failed to add process to scheduler");

		executor_terminate(
			embla->executor,
			process);

		process_manager_destroy_process(
			embla->process_manager,
			process_id);

		return NULL;
	}

	return process;
}

Process *embla_spawn(
	Embla *embla,
	const char *name,
	const char *path,
	char *const argv[])
{
	return embla_spawn_internal(
		embla,
		EMBLA_ROOT_PID,
		name,
		path,
		argv);
}

Process *embla_spawn_child(
	Embla *embla,
	Process *parent,
	const char *name,
	const char *path,
	char *const argv[])
{
	if (parent == NULL)
	{
		return NULL;
	}

	return embla_spawn_internal(
		embla,
		process_get_id(parent),
		name,
		path,
		argv);
}
