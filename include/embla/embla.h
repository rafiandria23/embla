#ifndef EMBLA_EMBLA_H
#define EMBLA_EMBLA_H

#include "embla/executor.h"
#include "embla/process.h"
#include "embla/process_config.h"
#include "embla/process_manager.h"
#include "embla/process_group_manager.h"
#include "embla/scheduler.h"

typedef struct Embla Embla;

typedef enum
{
	EMBLA_STOPPED,
	EMBLA_RUNNING,
	EMBLA_STOPPING
} EmblaState;

Embla *embla_create(void);

int embla_run(Embla *embla);

int embla_stop(Embla *embla);

void embla_destroy(Embla *embla);

ProcessManager *embla_process_manager(Embla *embla);
ProcessGroupManager *embla_process_group_manager(Embla *embla);
Scheduler *embla_scheduler(Embla *embla);
Executor *embla_executor(Embla *embla);

EmblaState embla_get_state(const Embla *embla);
const char *embla_state_name(EmblaState state);

Process *embla_spawn(
	Embla *embla,
	const char *name,
	const ProcessConfig *config);

Process *embla_spawn_child(
	Embla *embla,
	Process *parent,
	const char *name,
	const ProcessConfig *config);

int embla_terminate(Embla *embla, Process *process);
int embla_stop_process(Embla *embla, Process *process);
int embla_continue_process(Embla *embla, Process *process);
int embla_kill(Embla *embla, Process *process);

int embla_reap_child(
	Embla *embla,
	ProcessId parent_id,
	ProcessId *child_id);

int embla_signal_group(Embla *embla, ProcessGroup *group, int signal);
int embla_stop_group(Embla *embla, ProcessGroup *group);
int embla_continue_group(Embla *embla, ProcessGroup *group);
int embla_terminate_group(Embla *embla, ProcessGroup *group);
int embla_kill_group(Embla *embla, ProcessGroup *group);

#endif
