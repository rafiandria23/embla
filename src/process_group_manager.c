#include <stdlib.h>
#include <stdint.h>

#include "embla/log.h"
#include "embla/process_group_manager.h"

#define PROCESS_GROUP_MANAGER_INITIAL_CAPACITY 16

struct ProcessGroupManager
{
	ProcessGroup **groups;

	size_t count;
	size_t capacity;

	ProcessGroupId next_id;
};

ProcessGroupManager *process_group_manager_create(void)
{
	ProcessGroupManager *manager = malloc(sizeof(*manager));

	if (manager == NULL)
	{
		embla_log_error("failed to allocate process group manager");
		return NULL;
	}

	manager->groups = calloc(
		PROCESS_GROUP_MANAGER_INITIAL_CAPACITY,
		sizeof(*manager->groups));

	if (manager->groups == NULL)
	{
		embla_log_error("failed to allocate process group table");
		free(manager);
		return NULL;
	}

	manager->count = 0;
	manager->capacity = PROCESS_GROUP_MANAGER_INITIAL_CAPACITY;
	manager->next_id = 1;

	return manager;
}

static ProcessGroupId process_group_manager_allocate_id(
	ProcessGroupManager *manager)
{
	if (manager->next_id == EMBLA_INVALID_PGID)
	{
		return EMBLA_INVALID_PGID;
	}

	return manager->next_id++;
}

void process_group_manager_destroy(ProcessGroupManager *manager)
{
	if (manager == NULL)
	{
		return;
	}

	for (size_t i = 0; i < manager->count; i++)
	{
		process_group_destroy(manager->groups[i]);
	}

	free(manager->groups);
	free(manager);
}

ProcessGroup *process_group_manager_create_group(
	ProcessGroupManager *manager)
{
	if (manager == NULL)
	{
		return NULL;
	}

	if (manager->count >= manager->capacity)
	{
		embla_log_error("process group table is full");
		return NULL;
	}

	ProcessGroupId id = process_group_manager_allocate_id(manager);

	if (id == EMBLA_INVALID_PGID)
	{
		embla_log_error("process group ID space exhausted");
		return NULL;
	}

	ProcessGroup *group = process_group_create(id);

	if (group == NULL)
	{
		return NULL;
	}

	manager->groups[manager->count] = group;
	manager->count++;

	return group;
}

ProcessGroup *process_group_manager_get(
	const ProcessGroupManager *manager,
	ProcessGroupId id)
{
	if (
		manager == NULL ||
		id == EMBLA_INVALID_PGID)
	{
		return NULL;
	}

	for (size_t i = 0; i < manager->count; i++)
	{
		ProcessGroup *group = manager->groups[i];

		if (process_group_get_id(group) == id)
		{
			return group;
		}
	}

	return NULL;
}

size_t process_group_manager_count(const ProcessGroupManager *manager)
{
	if (manager == NULL)
	{
		return 0;
	}

	return manager->count;
}

int process_group_manager_destroy_group(
	ProcessGroupManager *manager,
	ProcessGroupId id)
{
	if (
		manager == NULL ||
		id == EMBLA_INVALID_PGID)
	{
		return -1;
	}

	for (size_t i = 0; i < manager->count; i++)
	{
		ProcessGroup *group = manager->groups[i];

		if (process_group_get_id(group) != id)
		{
			continue;
		}

		if (process_group_count(group) != 0)
		{
			embla_log_error("cannot destroy non-empty process group");
			return -1;
		}

		process_group_destroy(group);

		size_t last = manager->count - 1;

		manager->groups[i] = manager->groups[last];
		manager->groups[last] = NULL;

		manager->count--;

		return 0;
	}

	return -1;
}
