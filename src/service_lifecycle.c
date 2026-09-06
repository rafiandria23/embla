#include <stddef.h>

#include "embla/service_lifecycle.h"

int service_start(Service *service, Embla *embla)
{
	if (service == NULL || embla == NULL)
	{
		return -1;
	}

	if (service_get_state(service) != SERVICE_STOPPED)
	{
		return -1;
	}

	Process *process = embla_spawn(
		embla,
		service_get_name(service),
		service_get_config(service));

	if (process == NULL)
	{
		return -1;
	}

	if (service_set_process(service, process) != 0)
	{
		return -1;
	}

	if (service_set_last_start_time(service, service_monotonic_now()) != 0)
	{
		return -1;
	}

	if (service_set_permanently_failed(service, 0) != 0)
	{
		return -1;
	}

	if (service_set_state(service, SERVICE_RUNNING) != 0)
	{
		return -1;
	}

	return 0;
}

int service_stop(Service *service, Embla *embla)
{
	if (service == NULL || embla == NULL)
	{
		return -1;
	}

	if (service_get_state(service) != SERVICE_RUNNING)
	{
		return -1;
	}

	Process *process = service_get_process(service);

	if (process == NULL)
	{
		return -1;
	}

	if (service_set_stop_requested(service, 1) != 0)
	{
		return -1;
	}

	return embla_terminate(embla, process);
}

int service_reap(Service *service, Embla *embla)
{
	if (service == NULL || embla == NULL)
	{
		return -1;
	}

	if (service_get_state(service) != SERVICE_RUNNING)
	{
		return 0;
	}

	Process *process = service_get_process(service);

	if (process == NULL)
	{
		return -1;
	}

	if (process_get_state(process) != PROCESS_TERMINATED)
	{
		return 0;
	}

	if (service_set_last_exit_code(
			service,
			process_get_exit_code(process)) != 0)
	{
		return -1;
	}

	if (service_set_last_term_signal(
			service,
			process_get_term_signal(process)) != 0)
	{
		return -1;
	}

	ProcessId process_id = process_get_id(process);

	if (embla_reap_process(embla, process_id) != 0)
	{
		return -1;
	}

	if (service_set_process(service, NULL) != 0)
	{
		return -1;
	}

	if (service_set_state(service, SERVICE_STOPPED) != 0)
	{
		return -1;
	}

	return 1;
}
