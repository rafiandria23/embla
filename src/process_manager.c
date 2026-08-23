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

	manager->processes = calloc(
		PROCESS_MANAGER_INITIAL_CAPACITY,
		sizeof(*manager->processes));

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

Process *process_manager_create_process(
	ProcessManager *manager,
	ProcessId parent_id,
	ProcessGroupId group_id,
	const char *name)
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

	Process *process = process_create(
		id,
		parent_id,
		group_id,
		name);

	if (process == NULL)
	{
		return NULL;
	}

	manager->processes[manager->count] = process;
	manager->count++;

	return process;
}

Process *process_manager_get(
	const ProcessManager *manager,
	ProcessId id)
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

Process *process_manager_get_by_host_id(
	const ProcessManager *manager,
	HostProcessId host_id)
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

size_t process_manager_live_count(const ProcessManager *manager)
{
	if (manager == NULL)
	{
		return 0;
	}

	size_t count = 0;

	for (size_t i = 0; i < manager->count; i++)
	{
		Process *process = manager->processes[i];

		if (process_get_state(process) != PROCESS_TERMINATED)
		{
			count++;
		}
	}

	return count;
}

size_t process_manager_child_count(
	const ProcessManager *manager,
	ProcessId parent_id)
{
	if (
		manager == NULL ||
		parent_id == EMBLA_INVALID_PID)
	{
		return 0;
	}

	size_t count = 0;

	for (size_t i = 0; i < manager->count; i++)
	{
		Process *process = manager->processes[i];

		if (process_get_parent_id(process) == parent_id)
		{
			count++;
		}
	}

	return count;
}

int process_manager_destroy_process(
	ProcessManager *manager,
	ProcessId id)
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

Process *process_manager_wait_child(
	const ProcessManager *manager,
	ProcessId parent_id)
{
	if (
		manager == NULL ||
		parent_id == EMBLA_INVALID_PID)
	{
		return NULL;
	}

	for (size_t i = 0; i < manager->count; i++)
	{
		Process *process = manager->processes[i];

		if (
			process_get_parent_id(process) == parent_id &&
			process_get_state(process) == PROCESS_TERMINATED)
		{
			return process;
		}
	}

	return NULL;
}

int process_manager_reap_child(
	ProcessManager *manager,
	ProcessId parent_id,
	ProcessId *child_id)
{
	if (
		manager == NULL ||
		parent_id == EMBLA_INVALID_PID)
	{
		return -1;
	}

	Process *process = process_manager_wait_child(
		manager,
		parent_id);

	if (process == NULL)
	{
		return -1;
	}

	ProcessId id = process_get_id(process);

	if (child_id != NULL)
	{
		*child_id = id;
	}

	return process_manager_destroy_process(
		manager,
		id);
}

int process_manager_reparent_children(
	ProcessManager *manager,
	ProcessId parent_id,
	ProcessId new_parent_id)
{
	if (
		manager == NULL ||
		parent_id == EMBLA_INVALID_PID)
	{
		return -1;
	}

	ProcessManagerChildIterator *iterator =
		process_manager_child_iterator_create(
			manager,
			parent_id);

	if (iterator == NULL)
	{
		return -1;
	}

	Process *child;

	while (
		(child = process_manager_child_iterator_next(iterator)) != NULL)
	{
		if (process_get_state(child) == PROCESS_TERMINATED)
		{
			continue;
		}

		if (process_set_parent_id(
				child,
				new_parent_id) != 0)
		{
			process_manager_child_iterator_destroy(iterator);
			return -1;
		}
	}

	process_manager_child_iterator_destroy(iterator);

	return 0;
}

struct ProcessManagerChildIterator
{
	const ProcessManager *manager;
	ProcessId parent_id;
	size_t index;
};

ProcessManagerChildIterator *process_manager_child_iterator_create(
	const ProcessManager *manager,
	ProcessId parent_id)
{
	if (
		manager == NULL ||
		parent_id == EMBLA_INVALID_PID)
	{
		return NULL;
	}

	ProcessManagerChildIterator *iterator = malloc(sizeof(*iterator));

	if (iterator == NULL)
	{
		embla_log_error("failed to allocate child iterator");
		return NULL;
	}

	iterator->manager = manager;
	iterator->parent_id = parent_id;
	iterator->index = 0;

	return iterator;
}

void process_manager_child_iterator_destroy(
	ProcessManagerChildIterator *iterator)
{
	free(iterator);
}

Process *process_manager_child_iterator_next(
	ProcessManagerChildIterator *iterator)
{
	if (iterator == NULL)
	{
		return NULL;
	}

	const ProcessManager *manager = iterator->manager;

	while (iterator->index < manager->count)
	{
		Process *process = manager->processes[iterator->index];

		iterator->index++;

		if (
			process_get_parent_id(process) ==
			iterator->parent_id)
		{
			return process;
		}
	}

	return NULL;
}
