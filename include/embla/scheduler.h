#ifndef EMBLA_SCHEDULER_H
#define EMBLA_SCHEDULER_H

#include <stddef.h>

#include "embla/process.h"

typedef struct Scheduler Scheduler;

Scheduler *scheduler_create(void);
void scheduler_destroy(Scheduler *scheduler);

int scheduler_add(Scheduler *scheduler, Process *process);
int scheduler_remove(Scheduler *scheduler, Process *process);

Process *scheduler_next(Scheduler *scheduler);
Process *scheduler_current(const Scheduler *scheduler);

int scheduler_dispatch(Scheduler *scheduler);
int scheduler_yield(Scheduler *scheduler);

size_t scheduler_ready_count(const Scheduler *scheduler);

#endif
