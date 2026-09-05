#ifndef EMBLA_PROCESS_GROUP_H
#define EMBLA_PROCESS_GROUP_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include "embla/process.h"

#define EMBLA_INVALID_PGID UINT32_MAX
#define EMBLA_INVALID_HOST_PGID 0

typedef pid_t HostProcessGroupId;

typedef struct ProcessGroup ProcessGroup;

ProcessGroup *process_group_create(ProcessGroupId id);

void process_group_destroy(ProcessGroup *group);

ProcessGroupId process_group_get_id(const ProcessGroup *group);
size_t process_group_count(const ProcessGroup *group);

int process_group_add(ProcessGroup *group, Process *process);

int process_group_remove(ProcessGroup *group, Process *process);

HostProcessGroupId process_group_get_host_id(const ProcessGroup *group);

int process_group_set_host_id(
	ProcessGroup *group,
	HostProcessGroupId host_id);

int process_group_set_max_members(
	ProcessGroup *group,
	size_t max_members);

int process_group_has_max_members(const ProcessGroup *group);
size_t process_group_get_max_members(const ProcessGroup *group);

#endif
