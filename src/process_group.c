#include <stdlib.h>

#include "embla/log.h"
#include "embla/process_group.h"

#define PROCESS_GROUP_INITIAL_CAPACITY 16
#define PROCESS_GROUP_NOT_FOUND ((size_t)-1)

struct ProcessGroup
{
	ProcessGroupId id;
	HostProcessGroupId host_id;

	Process **processes;

	size_t count;
	size_t capacity;

	int has_max_members;
	size_t max_members;
};

ProcessGroup *process_group_create(ProcessGroupId id)
{
	if (id == EMBLA_INVALID_PGID)
	{
		return NULL;
	}

	ProcessGroup *group = malloc(sizeof(*group));

	if (group == NULL)
	{
		embla_log_error("failed to allocate process group");
		return NULL;
	}

	group->processes = calloc(
		PROCESS_GROUP_INITIAL_CAPACITY,
		sizeof(*group->processes));

	if (group->processes == NULL)
	{
		embla_log_error("failed to allocate process group members");

		free(group);

		return NULL;
	}

	group->id = id;
	group->host_id = EMBLA_INVALID_HOST_PGID;
	group->count = 0;
	group->capacity = PROCESS_GROUP_INITIAL_CAPACITY;
	group->has_max_members = 0;

	return group;
}

void process_group_destroy(ProcessGroup *group)
{
	if (group == NULL)
	{
		return;
	}

	free(group->processes);
	free(group);
}

ProcessGroupId process_group_get_id(const ProcessGroup *group)
{
	if (group == NULL)
	{
		return EMBLA_INVALID_PGID;
	}

	return group->id;
}

size_t process_group_count(const ProcessGroup *group)
{
	if (group == NULL)
	{
		return 0;
	}

	return group->count;
}

static size_t process_group_find_index(
	const ProcessGroup *group,
	const Process *process)
{
	for (size_t i = 0; i < group->count; i++)
	{
		if (group->processes[i] == process)
		{
			return i;
		}
	}

	return PROCESS_GROUP_NOT_FOUND;
}

int process_group_add(
	ProcessGroup *group,
	Process *process)
{
	if (group == NULL || process == NULL)
	{
		return -1;
	}

	if (group->count >= group->capacity)
	{
		embla_log_error("process group is full");
		return -1;
	}

	if (
		process_get_group_id(process) !=
		group->id)
	{
		embla_log_error("process group ID mismatch");
		return -1;
	}

	if (process_group_find_index(group, process) != PROCESS_GROUP_NOT_FOUND)
	{
		return -1;
	}

	group->processes[group->count] = process;
	group->count++;

	return 0;
}

int process_group_remove(
	ProcessGroup *group,
	Process *process)
{
	if (group == NULL || process == NULL)
	{
		return -1;
	}

	size_t index = process_group_find_index(group, process);

	if (index == PROCESS_GROUP_NOT_FOUND)
	{
		return -1;
	}

	size_t last = group->count - 1;

	group->processes[index] = group->processes[last];
	group->processes[last] = NULL;
	group->count--;

	return 0;
}

int process_group_set_host_id(
	ProcessGroup *group,
	HostProcessGroupId host_id)
{
	if (group == NULL)
	{
		return -1;
	}

	group->host_id = host_id;

	return 0;
}

HostProcessGroupId process_group_get_host_id(const ProcessGroup *group)
{
	if (group == NULL)
	{
		return EMBLA_INVALID_HOST_PGID;
	}

	return group->host_id;
}

int process_group_set_max_members(
	ProcessGroup *group,
	size_t max_members)
{
	if (group == NULL)
	{
		return -1;
	}

	group->has_max_members = 1;
	group->max_members = max_members;

	return 0;
}

int process_group_has_max_members(const ProcessGroup *group)
{
	if (group == NULL)
	{
		return 0;
	}

	return group->has_max_members;
}

size_t process_group_get_max_members(const ProcessGroup *group)
{
	if (group == NULL)
	{
		return 0;
	}

	return group->max_members;
}
