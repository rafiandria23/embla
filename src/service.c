#include <stdlib.h>

#include "embla/log.h"
#include "embla/service.h"
#include "embla/string.h"

struct Service
{
	char *name;
	ProcessConfig *config;
};

Service *service_create(
	const char *name,
	ProcessConfig *config)
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
