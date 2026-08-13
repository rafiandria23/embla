#ifndef EMBLA_PROCESS_H
#define EMBLA_PROCESS_H

#include <stdint.h>

#define EMBLA_INVALID_PID 0

typedef uint32_t ProcessId;

typedef enum
{
	PROCESS_CREATED,
	PROCESS_READY,
	PROCESS_RUNNING,
	PROCESS_BLOCKED,
	PROCESS_TERMINATED,
} ProcessState;

typedef struct Process Process;

Process *process_create(ProcessId id, const char *name);
void process_destroy(Process *process);

ProcessId process_get_id(const Process *process);
ProcessState process_get_state(const Process *process);

int process_set_state(Process *process, ProcessState state);
int process_transition(Process *process, ProcessState next_state);

const char *process_state_name(ProcessState state);

#endif
