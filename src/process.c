#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "embla/log.h"
#include "embla/process.h"
#include "embla/string.h"

struct Process
{
	ProcessId id;
	ProcessId parent_id;

	ProcessGroupId group_id;

	HostProcessId host_id;

	ProcessState state;

	char *name;

	int exit_code;
	int term_signal;
};

Process *process_create(
	ProcessId id,
	ProcessId parent_id,
	ProcessGroupId group_id,
	const char *name)
{
	if (id == EMBLA_INVALID_PID || name == NULL)
	{
		return NULL;
	}

	Process *process = malloc(sizeof(*process));

	if (process == NULL)
	{
		embla_log_error("failed to allocate process");
		return NULL;
	}

	process->id = id;
	process->parent_id = parent_id;
	process->group_id = group_id;
	process->state = PROCESS_CREATED;

	process->name = embla_strdup(name);

	if (process->name == NULL)
	{
		embla_log_error("failed to allocate process name");
		free(process);
		return NULL;
	}

	process->exit_code = -1;
	process->term_signal = -1;

	return process;
}

void process_destroy(Process *process)
{
	if (process == NULL)
	{
		return;
	}

	free(process->name);
	free(process);
}

ProcessId process_get_id(const Process *process)
{
	if (process == NULL)
	{
		return 0;
	}

	return process->id;
}

ProcessId process_get_parent_id(const Process *process)
{
	if (process == NULL)
	{
		return EMBLA_INVALID_PID;
	}

	return process->parent_id;
}

HostProcessId process_get_host_id(const Process *process)
{
	if (process == NULL)
	{
		return EMBLA_INVALID_HOST_PID;
	}

	return process->host_id;
}

int process_set_host_id(Process *process, HostProcessId host_id)
{
	if (process == NULL)
	{
		return -1;
	}

	process->host_id = host_id;

	return 0;
}

ProcessState process_get_state(const Process *process)
{
	if (process == NULL)
	{
		return PROCESS_TERMINATED;
	}

	return process->state;
}

static int process_can_transition(
	ProcessState current,
	ProcessState next)
{
	switch (current)
	{
	case PROCESS_CREATED:
		return next == PROCESS_READY;

	case PROCESS_READY:
		return next == PROCESS_RUNNING ||
			   next == PROCESS_TERMINATED;

	case PROCESS_RUNNING:
		return next == PROCESS_READY ||
			   next == PROCESS_STOPPED ||
			   next == PROCESS_BLOCKED ||
			   next == PROCESS_TERMINATED;

	case PROCESS_STOPPED:
		return next == PROCESS_RUNNING ||
			   next == PROCESS_TERMINATED;

	case PROCESS_BLOCKED:
		return next == PROCESS_READY ||
			   next == PROCESS_TERMINATED;

	case PROCESS_TERMINATED:
		return 0;
	}

	return 0;
}

int process_transition(Process *process, ProcessState next_state)
{
	if (process == NULL)
	{
		return -1;
	}

	if (!process_can_transition(process->state, next_state))
	{
		return -1;
	}

	process->state = next_state;

	return 0;
}

const char *process_state_name(ProcessState state)
{
	switch (state)
	{
	case PROCESS_CREATED:
		return "CREATED";

	case PROCESS_READY:
		return "READY";

	case PROCESS_RUNNING:
		return "RUNNING";

	case PROCESS_STOPPED:
		return "STOPPED";

	case PROCESS_BLOCKED:
		return "BLOCKED";

	case PROCESS_TERMINATED:
		return "TERMINATED";
	}

	return "UNKNOWN";
}

int process_get_exit_code(const Process *process)
{
	if (process == NULL)
	{
		return -1;
	}

	return process->exit_code;
}

int process_set_exit_code(Process *process, int exit_code)
{
	if (process == NULL)
	{
		return -1;
	}

	process->exit_code = exit_code;

	return 0;
}

int process_get_term_signal(const Process *process)
{
	if (process == NULL)
	{
		return -1;
	}

	return process->term_signal;
}

int process_set_term_signal(Process *process, int term_signal)
{
	if (process == NULL)
	{
		return -1;
	}

	process->term_signal = term_signal;

	return 0;
}

int process_set_parent_id(Process *process, ProcessId parent_id)
{
	if (process == NULL)
	{
		return -1;
	}

	process->parent_id = parent_id;

	return 0;
}
