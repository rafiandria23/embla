#ifndef EMBLA_EXECUTOR_H
#define EMBLA_EXECUTOR_H

#include "embla/process.h"

typedef struct Executor Executor;

Executor *executor_create(void);
void executor_destroy(Executor *executor);

int executor_spawn(
	Executor *executor,
	Process *process,
	const char *path,
	char *const argv[]);

int executor_terminate(Executor *executor, Process *process);
int executor_wait(Executor *executor, Process *process, int *status);
int executor_poll(Executor *executor, Process *process);
int executor_poll_any(Executor *executor, HostProcessId *host_id, int *status);

#endif