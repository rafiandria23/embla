#ifndef EMBLA_PROCESS_H
#define EMBLA_PROCESS_H

#include <stdint.h>
#include <sys/types.h>

#define EMBLA_ROOT_PID 0
#define EMBLA_INVALID_PID UINT32_MAX
#define EMBLA_INVALID_HOST_PID 0

typedef pid_t HostProcessId;

typedef uint32_t ProcessId;
typedef uint32_t ProcessGroupId;

typedef enum
{
	PROCESS_CREATED,
	PROCESS_READY,
	PROCESS_RUNNING,
	PROCESS_STOPPED,
	PROCESS_BLOCKED,
	PROCESS_TERMINATED,
} ProcessState;

typedef struct Process Process;

Process *process_create(
	ProcessId id,
	ProcessId parent_id,
	ProcessGroupId group_id,
	const char *name);

void process_destroy(Process *process);

ProcessId process_get_id(const Process *process);

int process_set_parent_id(Process *process, ProcessId parent_id);
ProcessId process_get_parent_id(const Process *process);

int process_set_group_id(Process *process, ProcessGroupId group_id);
ProcessGroupId process_get_group_id(const Process *process);

int process_set_host_id(Process *process, HostProcessId host_id);
HostProcessId process_get_host_id(const Process *process);

ProcessState process_get_state(const Process *process);

int process_transition(Process *process, ProcessState next_state);

const char *process_state_name(ProcessState state);

int process_set_exit_code(Process *process, int exit_code);
int process_get_exit_code(const Process *process);

int process_set_term_signal(Process *process, int term_signal);
int process_get_term_signal(const Process *process);

int process_set_cpu_user_seconds(Process *process, double seconds);
double process_get_cpu_user_seconds(const Process *process);

int process_set_cpu_system_seconds(Process *process, double seconds);
double process_get_cpu_system_seconds(const Process *process);

int process_set_max_rss_bytes(Process *process, long bytes);
long process_get_max_rss_bytes(const Process *process);

#endif
