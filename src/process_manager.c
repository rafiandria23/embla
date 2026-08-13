#include <stdlib.h>
#include <stdint.h>

#include "embla/log.h"
#include "embla/process_manager.h"

#define PROCESS_MANAGER_INITIAL_CAPACITY 16

struct ProcessManager
{
	Process **processes;

	size_t count;
	size_t capacity;

	ProcessId next_pid;

	size_t iterator_index;
};

ProcessManager *process_manager_create(void)
{
	ProcessManager *manager = malloc(sizeof(*manager));

	if (manager == NULL)
	{
		embla_log_error("failed to allocate process manager");
		return NULL;
	}

	manager->processes = calloc(PROCESS_MANAGER_INITIAL_CAPACITY, sizeof(*manager->processes));

	if (manager->processes == NULL)
	{
		embla_log_error("failed to allocate process table");
		free(manager);
		return NULL;
	}

	manager->count = 0;
	manager->capacity = PROCESS_MANAGER_INITIAL_CAPACITY;
	manager->next_pid = 1;
	manager->iterator_index = 0;

	return manager;
}

void process_manager_destroy(ProcessManager *manager)
{
	if (manager == NULL)
	{
		return;
	}

	for (size_t i = 0; i < manager->count; i++)
	{
		process_destroy(manager->processes[i]);
	}

	free(manager->processes);
	free(manager);
}

static ProcessId process_manager_allocate_pid(ProcessManager *manager)
{
	if (manager->next_pid == UINT32_MAX)
	{
		return EMBLA_INVALID_PID;
	}

	return manager->next_pid++;
}

Process *process_manager_create_process(ProcessManager *manager, const char *name)
{
	if (manager == NULL || name == NULL)
	{
		return NULL;
	}

	if (manager->count >= manager->capacity)
	{
		embla_log_error("process table is full");
		return NULL;
	}

	ProcessId id = process_manager_allocate_pid(manager);

	if (id == EMBLA_INVALID_PID)
	{
		embla_log_error("PID space exhausted");
		return NULL;
	}

	Process *process = process_create(id, name);

	if (process == NULL)
	{
		return NULL;
	}

	manager->processes[manager->count] = process;
	manager->count++;

	return process;
}

Process *process_manager_get(const ProcessManager *manager, ProcessId id)
{
	if (manager == NULL)
	{
		return NULL;
	}

	for (size_t i = 0; i < manager->count; i++)
	{
		Process *process = manager->processes[i];

		if (process_get_id(process) == id)
		{
			return process;
		}
	}

	return NULL;
}

Process *process_manager_get_by_host_id(const ProcessManager *manager, HostProcessId host_id)
{
	if (manager == NULL || host_id == EMBLA_INVALID_HOST_PID)
	{
		return NULL;
	}

	for (size_t i = 0; i < manager->count; i++)
	{
		Process *process = manager->processes[i];

		if (process_get_host_id(process) == host_id)
		{
			return process;
		}
	}

	return NULL;
}

size_t process_manager_count(const ProcessManager *manager)
{
	if (manager == NULL)
	{
		return 0;
	}

	return manager->count;
}

int process_manager_destroy_process(ProcessManager *manager, ProcessId id)
{
	if (manager == NULL || id == EMBLA_INVALID_PID)
	{
		return -1;
	}

	for (size_t i = 0; i < manager->count; i++)
	{
		Process *process = manager->processes[i];

		if (process_get_id(process) != id)
		{
			continue;
		}

		process_destroy(process);

		size_t last = manager->count - 1;

		manager->processes[i] = manager->processes[last];
		manager->processes[last] = NULL;

		manager->count--;
		manager->iterator_index = 0;

		return 0;
	}

	return -1;
}

Process *process_manager_first(ProcessManager *manager)
{
	if (manager == NULL || manager->count == 0)
	{
		return NULL;
	}

	manager->iterator_index = 0;

	return manager->processes[manager->iterator_index];
}

Process *process_manager_next(ProcessManager *manager)
{
	if (manager == NULL)
	{
		return NULL;
	}

	if (manager->iterator_index + 1 >= manager->count)
	{
		return NULL;
	}

	manager->iterator_index++;

	return manager->processes[manager->iterator_index];
}
