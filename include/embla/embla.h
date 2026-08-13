#ifndef EMBLA_EMBLA_H
#define EMBLA_EMBLA_H

typedef struct Embla Embla;
typedef struct ProcessManager ProcessManager;
typedef struct Scheduler Scheduler;
typedef struct Executor Executor;

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
Scheduler *embla_scheduler(Embla *embla);
Executor *embla_executor(Embla *embla);

EmblaState embla_get_state(const Embla *embla);
const char *embla_state_name(EmblaState state);

int embla_spawn(
	Embla *embla,
	const char *name,
	const char *path,
	char *const argv[]);

#endif
