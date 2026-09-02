#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#include "embla/embla.h"
#include "embla/log.h"
#include "embla/process_manager.h"
#include "embla/process_group_manager.h"
#include "embla/scheduler.h"
#include "embla/executor.h"

struct Embla
{
	ProcessManager *process_manager;
	ProcessGroupManager *process_group_manager;
	Scheduler *scheduler;
	Executor *executor;

	EmblaState state;
};

static int embla_reap_root_children(Embla *embla);

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

	embla->process_group_manager = process_group_manager_create();

	if (embla->process_group_manager == NULL)
	{
		embla_log_error("failed to create process group manager");

		process_manager_destroy(embla->process_manager);
		free(embla);

		return NULL;
	}

	embla->scheduler = scheduler_create();

	if (embla->scheduler == NULL)
	{
		embla_log_error("failed to create scheduler");

		process_group_manager_destroy(embla->process_group_manager);
		process_manager_destroy(embla->process_manager);

		free(embla);

		return NULL;
	}

	embla->executor = executor_create();

	if (embla->executor == NULL)
	{
		embla_log_error("failed to create executor");

		scheduler_destroy(embla->scheduler);
		process_group_manager_destroy(embla->process_group_manager);
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

	if (embla_reap_root_children(embla) != 0)
	{
		embla_log_error("failed to reap root children");
		return -1;
	}

	embla->state = EMBLA_STOPPED;

	return 0;
}

static int embla_handle_child_event(
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
		embla_log_error("received event for unknown process");
		return -1;
	}

	if (WIFEXITED(wait_status))
	{
		if (process_set_exit_code(
				process,
				WEXITSTATUS(wait_status)) != 0)
		{
			return -1;
		}

		goto terminated;
	}

	if (WIFSIGNALED(wait_status))
	{
		if (process_set_term_signal(
				process,
				WTERMSIG(wait_status)) != 0)
		{
			return -1;
		}

		goto terminated;
	}

	if (WIFSTOPPED(wait_status))
	{
		return process_transition(
			process,
			PROCESS_STOPPED);
	}

	if (WIFCONTINUED(wait_status))
	{
		return process_transition(
			process,
			PROCESS_RUNNING);
	}

	return 0;

terminated:
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
		embla_log_error("failed to remove terminated process");
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
			if (embla_handle_child_event(
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
	process_group_manager_destroy(embla->process_group_manager);
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

ProcessGroupManager *embla_process_group_manager(Embla *embla)
{
	if (embla == NULL)
	{
		return NULL;
	}

	return embla->process_group_manager;
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
	ProcessGroupId group_id,
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

	ProcessGroup *group = process_group_manager_get(
		embla->process_group_manager,
		group_id);

	if (group == NULL)
	{
		embla_log_error("failed to find process group");
		return NULL;
	}

	Process *process = process_manager_create_process(
		embla->process_manager,
		parent_id,
		group_id,
		name);

	if (process == NULL)
	{
		embla_log_error("failed to create process");
		return NULL;
	}

	ProcessId process_id = process_get_id(process);

	if (process_group_add(
			group,
			process) != 0)
	{
		embla_log_error("failed to add process to process group");

		process_manager_destroy_process(
			embla->process_manager,
			process_id);

		return NULL;
	}

	HostProcessGroupId target_host_group_id = process_group_get_host_id(group);

	if (executor_spawn(
			embla->executor,
			process,
			target_host_group_id,
			path,
			argv) != 0)
	{
		embla_log_error("failed to spawn host process");

		process_group_remove(
			group,
			process);

		process_manager_destroy_process(
			embla->process_manager,
			process_id);

		return NULL;
	}

	if (target_host_group_id == EMBLA_INVALID_HOST_PGID)
	{
		HostProcessGroupId leader_host_group_id = (HostProcessGroupId)process_get_host_id(process);

		if (process_group_set_host_id(group, leader_host_group_id) != 0)
		{
			embla_log_error("failed to record host PGID for process group");

			executor_terminate(
				embla->executor,
				process);

			process_group_remove(
				group,
				process);

			process_manager_destroy_process(
				embla->process_manager,
				process_id);

			return NULL;
		}
	}

	if (process_transition(
			process,
			PROCESS_READY) != 0)
	{
		embla_log_error("failed to transition process to READY");

		executor_terminate(
			embla->executor,
			process);

		process_group_remove(
			group,
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

		process_group_remove(
			group,
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
	if (
		embla == NULL ||
		name == NULL ||
		path == NULL ||
		argv == NULL)
	{
		return NULL;
	}

	ProcessGroup *group = process_group_manager_create_group(
		embla->process_group_manager);

	if (group == NULL)
	{
		embla_log_error("failed to create process group");
		return NULL;
	}

	ProcessGroupId group_id = process_group_get_id(group);

	Process *process = embla_spawn_internal(
		embla,
		EMBLA_ROOT_PID,
		group_id,
		name,
		path,
		argv);

	if (process == NULL)
	{
		if (process_group_manager_destroy_group(
				embla->process_group_manager,
				group_id) != 0)
		{
			embla_log_error("failed to clean up process group");
		}

		return NULL;
	}

	return process;
}

Process *embla_spawn_child(
	Embla *embla,
	Process *parent,
	const char *name,
	const char *path,
	char *const argv[])
{
	if (
		embla == NULL ||
		parent == NULL ||
		name == NULL ||
		path == NULL ||
		argv == NULL)
	{
		return NULL;
	}

	ProcessGroupId group_id = process_get_group_id(parent);

	if (group_id == EMBLA_INVALID_PGID)
	{
		embla_log_error("parent has invalid process group");
		return NULL;
	}

	return embla_spawn_internal(
		embla,
		process_get_id(parent),
		group_id,
		name,
		path,
		argv);
}

int embla_terminate(
	Embla *embla,
	Process *process)
{
	if (
		embla == NULL ||
		process == NULL)
	{
		return -1;
	}

	if (process_get_state(process) == PROCESS_TERMINATED)
	{
		return -1;
	}

	return executor_signal(
		embla->executor,
		process,
		SIGTERM);
}

int embla_stop_process(
	Embla *embla,
	Process *process)
{
	if (
		embla == NULL ||
		process == NULL)
	{
		return -1;
	}

	ProcessState state = process_get_state(process);

	if (
		state == PROCESS_TERMINATED ||
		state == PROCESS_STOPPED)
	{
		return -1;
	}

	return executor_signal(
		embla->executor,
		process,
		SIGSTOP);
}

int embla_continue_process(
	Embla *embla,
	Process *process)
{
	if (embla == NULL || process == NULL)
	{
		return -1;
	}

	if (
		process_get_state(process) !=
		PROCESS_STOPPED)
	{
		return -1;
	}

	return executor_signal(
		embla->executor,
		process,
		SIGCONT);
}

int embla_kill(
	Embla *embla,
	Process *process)
{
	if (embla == NULL || process == NULL)
	{
		return -1;
	}

	if (process_get_state(process) == PROCESS_TERMINATED)
	{
		return -1;
	}

	return executor_signal(
		embla->executor,
		process,
		SIGKILL);
}

int embla_reap_child(
	Embla *embla,
	ProcessId parent_id,
	ProcessId *child_id)
{
	if (embla == NULL)
	{
		return -1;
	}

	Process *process = process_manager_wait_child(
		embla->process_manager,
		parent_id);

	if (process == NULL)
	{
		return -1;
	}

	ProcessId process_id = process_get_id(process);

	ProcessGroupId group_id = process_get_group_id(process);

	ProcessGroup *group = process_group_manager_get(
		embla->process_group_manager,
		group_id);

	if (group == NULL)
	{
		embla_log_error("failed to find process group while reaping child");
		return -1;
	}

	if (process_group_remove(
			group,
			process) != 0)
	{
		embla_log_error("failed to remove process from process group");
		return -1;
	}

	bool group_empty = process_group_count(group) == 0;

	if (process_manager_destroy_process(
			embla->process_manager,
			process_id) != 0)
	{
		embla_log_error(
			"failed to destroy reaped process");

		return -1;
	}

	if (group_empty)
	{
		if (process_group_manager_destroy_group(
				embla->process_group_manager,
				group_id) != 0)
		{
			embla_log_error(
				"failed to destroy empty process group");

			return -1;
		}
	}

	if (child_id != NULL)
	{
		*child_id = process_id;
	}

	return 0;
}

static int embla_reap_root_children(Embla *embla)
{
	if (embla == NULL)
	{
		return -1;
	}

	for (;;)
	{
		Process *child = process_manager_wait_child(
			embla->process_manager,
			EMBLA_ROOT_PID);

		if (child == NULL)
		{
			return 0;
		}

		ProcessId child_id;

		if (embla_reap_child(
				embla,
				EMBLA_ROOT_PID,
				&child_id) != 0)
		{
			embla_log_error("failed to reap root child");
			return -1;
		}
	}
}

int embla_signal_group(
	Embla *embla,
	ProcessGroup *group,
	int signal)
{
	if (embla == NULL || group == NULL)
	{
		return -1;
	}

	HostProcessGroupId host_group_id = process_group_get_host_id(group);

	if (host_group_id == EMBLA_INVALID_HOST_PGID)
	{
		return -1;
	}

	return executor_signal_group(
		embla->executor,
		host_group_id,
		signal);
}

int embla_stop_group(
	Embla *embla,
	ProcessGroup *group)
{
	if (embla == NULL || group == NULL)
	{
		return -1;
	}

	return embla_signal_group(
		embla,
		group,
		SIGSTOP);
}

int embla_continue_group(
	Embla *embla,
	ProcessGroup *group)
{
	if (embla == NULL || group == NULL)
	{
		return -1;
	}

	return embla_signal_group(
		embla,
		group,
		SIGCONT);
}

int embla_terminate_group(
	Embla *embla,
	ProcessGroup *group)
{
	if (embla == NULL || group == NULL)
	{
		return -1;
	}

	return embla_signal_group(
		embla,
		group,
		SIGTERM);
}

int embla_kill_group(
	Embla *embla,
	ProcessGroup *group)
{
	if (embla == NULL || group == NULL)
	{
		return -1;
	}

	return embla_signal_group(
		embla,
		group,
		SIGKILL);
}
