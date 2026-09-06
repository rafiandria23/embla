#include <stdlib.h>

#include "embla/log.h"
#include "embla/service.h"
#include "embla/string.h"

struct Service
{
	char *name;
	ProcessConfig *config;

	ServiceState state;
	Process *process;

	int last_exit_code;
	int last_term_signal;
};

Service *service_create(const char *name, ProcessConfig *config)
{
	if (
		name == NULL ||
		config == NULL)
	{
		return NULL;
	}

	Service *service = malloc(sizeof(*service));

	if (service == NULL)
	{
		embla_log_error("failed to allocate service");
		return NULL;
	}

	service->name = embla_strdup(name);

	if (service->name == NULL)
	{
		embla_log_error("failed to duplicate service name");

		free(service);

		return NULL;
	}

	service->config = config;
	service->state = SERVICE_STOPPED;
	service->process = NULL;
	service->last_exit_code = -1;
	service->last_term_signal = -1;

	return service;
}

void service_destroy(Service *service)
{
	if (service == NULL)
	{
		return;
	}

	free(service->name);
	process_config_destroy(service->config);
	free(service);
}

const char *service_get_name(const Service *service)
{
	if (service == NULL)
	{
		return NULL;
	}

	return service->name;
}

const ProcessConfig *service_get_config(const Service *service)
{
	if (service == NULL)
	{
		return NULL;
	}

	return service->config;
}

int service_set_state(Service *service, ServiceState state)
{
	if (service == NULL)
	{
		return -1;
	}

	service->state = state;

	return 0;
}

ServiceState service_get_state(const Service *service)
{
	if (service == NULL)
	{
		return SERVICE_STOPPED;
	}

	return service->state;
}

int service_set_process(Service *service, Process *process)
{
	if (service == NULL)
	{
		return -1;
	}

	service->process = process;

	return 0;
}

Process *service_get_process(const Service *service)
{
	if (service == NULL)
	{
		return NULL;
	}

	return service->process;
}

int service_set_last_exit_code(Service *service, int exit_code)
{
	if (service == NULL)
	{
		return -1;
	}

	service->last_exit_code = exit_code;

	return 0;
}

int service_get_last_exit_code(const Service *service)
{
	if (service == NULL)
	{
		return -1;
	}

	return service->last_exit_code;
}

int service_set_last_term_signal(Service *service, int term_signal)
{
	if (service == NULL)
	{
		return -1;
	}

	service->last_term_signal = term_signal;

	return 0;
}

int service_get_last_term_signal(const Service *service)
{
	if (service == NULL)
	{
		return -1;
	}

	return service->last_term_signal;
}
