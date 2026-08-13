#ifndef EMBLA_PROCESS_MANAGER_H
#define EMBLA_PROCESS_MANAGER_H

#include <stddef.h>

#include "embla/process.h"

typedef struct ProcessManager ProcessManager;

ProcessManager *process_manager_create(void);
void process_manager_destroy(ProcessManager *manager);

Process *process_manager_create_process(ProcessManager *manager, const char *name);

/*
 * Returns a borrowed pointer.
 *
 * The returned Process is owned by the ProcessManager
 * and becomes invalid when that process is destroyed.
 */
Process *process_manager_get(const ProcessManager *manager, ProcessId id);
Process *process_manager_get_by_host_id(const ProcessManager *manager, HostProcessId host_id);

size_t process_manager_count(const ProcessManager *manager);

int process_manager_destroy_process(ProcessManager *manager, ProcessId id);

Process *process_manager_first(ProcessManager *manager);

Process *process_manager_next(ProcessManager *manager);

#endif
