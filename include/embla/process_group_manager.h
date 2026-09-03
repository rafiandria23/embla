#ifndef EMBLA_PROCESS_GROUP_MANAGER_H
#define EMBLA_PROCESS_GROUP_MANAGER_H

#include <stddef.h>

#include "embla/process_group.h"

typedef struct ProcessGroupManager ProcessGroupManager;

ProcessGroupManager *process_group_manager_create(void);

void process_group_manager_destroy(ProcessGroupManager *manager);

ProcessGroup *process_group_manager_create_group(
	ProcessGroupManager *manager);

ProcessGroup *process_group_manager_get(
	const ProcessGroupManager *manager,
	ProcessGroupId id);

size_t process_group_manager_count(const ProcessGroupManager *manager);

int process_group_manager_destroy_group(
	ProcessGroupManager *manager,
	ProcessGroupId id);

#endif
