#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "embla/log.h"
#include "embla/process.h"
#include "embla/string.h"

struct Process
{
	ProcessId id;
	ProcessState state;
	char *name;
};

Process *process_create(ProcessId id, const char *name)
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
	process->state = PROCESS_CREATED;

	process->name = embla_strdup(name);

	if (process->name == NULL)
	{
		embla_log_error("failed to allocate process name");
		free(process);
		return NULL;
	}

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

ProcessState process_get_state(const Process *process)
{
	if (process == NULL)
	{
		return PROCESS_TERMINATED;
	}

	return process->state;
}

int process_set_state(Process *process, ProcessState state)
{
	if (process == NULL)
	{
		return -1;
	}

	process->state = state;

	return 0;
}

static int process_can_transition(ProcessState current, ProcessState next)
{
	switch (current)
	{
	case PROCESS_CREATED:
		return next == PROCESS_READY;

	case PROCESS_READY:
		return next == PROCESS_RUNNING;

	case PROCESS_RUNNING:
		return next == PROCESS_READY || next == PROCESS_BLOCKED || next == PROCESS_TERMINATED;

	case PROCESS_BLOCKED:
		return next == PROCESS_READY || next == PROCESS_TERMINATED;

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

	case PROCESS_BLOCKED:
		return "BLOCKED";

	case PROCESS_TERMINATED:
		return "TERMINATED";
	}

	return "UNKNOWN";
}
