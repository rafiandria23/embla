#ifndef EMBLA_EMBLA_H
#define EMBLA_EMBLA_H

typedef struct Embla Embla;
typedef struct ProcessManager ProcessManager;
typedef struct Scheduler Scheduler;

Embla *embla_create(void);
int embla_run(Embla *embla);
void embla_destroy(Embla *embla);

ProcessManager *embla_process_manager(Embla *embla);
Scheduler *embla_scheduler(Embla *embla);

#endif
