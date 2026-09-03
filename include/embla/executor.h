#ifndef EMBLA_EXECUTOR_H
#define EMBLA_EXECUTOR_H

#include "embla/process.h"
#include "embla/process_config.h"
#include "embla/process_group.h"

/*
 * Owns host-process mechanics: forking, exec'ing, signaling, and
 * reaping real OS processes on Embla's behalf. Opaque; the
 * concrete struct is private to src/executor.c.
 */
typedef struct Executor Executor;

Executor *executor_create(void);
void executor_destroy(Executor *executor);

int executor_spawn(
	Executor *executor,
	Process *process,
	HostProcessGroupId target_host_group_id,
	const ProcessConfig *config);

int executor_terminate(Executor *executor, Process *process);

int executor_wait(
	Executor *executor,
	Process *process,
	int *status);

int executor_poll(Executor *executor, Process *process);

int executor_poll_any(
	Executor *executor,
	HostProcessId *host_id,
	int *status);

int executor_signal(
	Executor *executor,
	Process *process,
	int signal);

int executor_signal_group(
	Executor *executor,
	HostProcessGroupId host_group_id,
	int signal);

#endif
