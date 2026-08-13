#include <stdio.h>

#include "embla/embla.h"
#include "embla/process.h"
#include "embla/process_manager.h"
#include "embla/scheduler.h"

int main(void)
{
	Embla *embla = embla_create();

	if (embla == NULL)
	{
		return 1;
	}

	ProcessManager *process_manager = embla_process_manager(embla);

	Scheduler *scheduler = embla_scheduler(embla);

	Process *init = process_manager_create_process(process_manager, "init");

	Process *shell = process_manager_create_process(process_manager, "shell");

	process_transition(init, PROCESS_READY);
	process_transition(shell, PROCESS_READY);

	scheduler_add(scheduler, init);
	scheduler_add(scheduler, shell);

	scheduler_dispatch(scheduler);

	printf("running PID: %u\n", process_get_id(scheduler_current(scheduler)));

	scheduler_yield(scheduler);
	scheduler_dispatch(scheduler);

	printf("running PID: %u\n", process_get_id(scheduler_current(scheduler)));

	embla_destroy(embla);

	return 0;
}