#ifndef EMBLA_PROCESS_MANAGER_H
#define EMBLA_PROCESS_MANAGER_H

#include <stddef.h>

#include "embla/process.h"

typedef struct ProcessManager ProcessManager;

ProcessManager *process_manager_create(void);

void process_manager_destroy(ProcessManager *manager);

Process *process_manager_create_process(
	ProcessManager *manager,
	ProcessId parent_id,
	ProcessGroupId group_id,
	const char *name);

Process *process_manager_get(const ProcessManager *manager, ProcessId id);
Process *process_manager_get_by_host_id(const ProcessManager *manager, HostProcessId host_id);

size_t process_manager_count(const ProcessManager *manager);

size_t process_manager_live_count(const ProcessManager *manager);

size_t process_manager_child_count(
	const ProcessManager *manager,
	ProcessId parent_id);

int process_manager_destroy_process(ProcessManager *manager, ProcessId id);

Process *process_manager_first(ProcessManager *manager);
Process *process_manager_next(ProcessManager *manager);

Process *process_manager_wait_child(
	const ProcessManager *manager,
	ProcessId parent_id);

int process_manager_reap_child(
	ProcessManager *manager,
	ProcessId parent_id,
	ProcessId *child_id);

int process_manager_reparent_children(
	ProcessManager *manager,
	ProcessId parent_id,
	ProcessId new_parent_id);

typedef struct ProcessManagerChildIterator ProcessManagerChildIterator;

ProcessManagerChildIterator *process_manager_child_iterator_create(
	const ProcessManager *manager,
	ProcessId parent_id);

void process_manager_child_iterator_destroy(
	ProcessManagerChildIterator *iterator);

Process *process_manager_child_iterator_next(
	ProcessManagerChildIterator *iterator);

#endif
