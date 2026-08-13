#ifndef EMBLA_EXECUTOR_H
#define EMBLA_EXECUTOR_H

#include "embla/process.h"

typedef struct Executor Executor;

Executor *executor_create(void);
void executor_destroy(Executor *executor);

int executor_spawn(Executor *executor, Process *process);

int executor_terminate(Executor *executor, Process *process);

#endif
