#ifndef EMBLA_PROCESS_GROUP_H
#define EMBLA_PROCESS_GROUP_H

#include <stddef.h>

#include "embla/process.h"

typedef struct ProcessGroup ProcessGroup;

ProcessGroup *process_group_create(ProcessGroupId id);
void process_group_destroy(ProcessGroup *group);

ProcessGroupId process_group_get_id(const ProcessGroup *group);
size_t process_group_count(const ProcessGroup *group);

int process_group_add(ProcessGroup *group, Process *process);
int process_group_remove(ProcessGroup *group, Process *process);

#endif
