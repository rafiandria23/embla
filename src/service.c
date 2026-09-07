#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "embla/log.h"
#include "embla/service.h"
#include "embla/string.h"

#define SERVICE_MAX_DEPENDENCIES 16

struct Service
{
	char *name;
	ProcessConfig *config;

	ServiceState state;
	Process *process;

	int last_exit_code;
	int last_term_signal;

	RestartPolicy restart_policy;
	double restart_base_delay_seconds;
	double restart_max_delay_seconds;
	double restart_stability_threshold_seconds;
	int max_consecutive_restarts;

	int stop_requested;
	int consecutive_restart_count;
	int permanently_failed;
	int restart_pending;
	double last_start_time;
	double restart_allowed_at;

	char *dependencies[SERVICE_MAX_DEPENDENCIES];
	int dependency_count;
};

Service *service_create(const char *name, ProcessConfig *config)
{
	if (name == NULL || config == NULL)
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

	service->restart_policy = RESTART_NEVER;
	service->restart_base_delay_seconds = 1.0;
	service->restart_max_delay_seconds = 300.0;
	service->restart_stability_threshold_seconds = 60.0;
	service->max_consecutive_restarts = 5;

	service->stop_requested = 0;
	service->consecutive_restart_count = 0;
	service->permanently_failed = 0;
	service->restart_pending = 0;
	service->last_start_time = 0.0;
	service->restart_allowed_at = 0.0;

	service->dependency_count = 0;

	return service;
}

void service_destroy(Service *service)
{
	if (service == NULL)
	{
		return;
	}

	for (int i = 0; i < service->dependency_count; i++)
	{
		free(service->dependencies[i]);
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

int service_set_restart_policy(Service *service, RestartPolicy policy)
{
	if (service == NULL)
	{
		return -1;
	}

	service->restart_policy = policy;

	return 0;
}

RestartPolicy service_get_restart_policy(const Service *service)
{
	if (service == NULL)
	{
		return RESTART_NEVER;
	}

	return service->restart_policy;
}

int service_set_restart_base_delay(Service *service, double seconds)
{
	if (service == NULL)
	{
		return -1;
	}

	service->restart_base_delay_seconds = seconds;

	return 0;
}

double service_get_restart_base_delay(const Service *service)
{
	if (service == NULL)
	{
		return -1.0;
	}

	return service->restart_base_delay_seconds;
}

int service_set_restart_max_delay(Service *service, double seconds)
{
	if (service == NULL)
	{
		return -1;
	}

	service->restart_max_delay_seconds = seconds;

	return 0;
}

double service_get_restart_max_delay(const Service *service)
{
	if (service == NULL)
	{
		return -1.0;
	}

	return service->restart_max_delay_seconds;
}

int service_set_restart_stability_threshold(Service *service, double seconds)
{
	if (service == NULL)
	{
		return -1;
	}

	service->restart_stability_threshold_seconds = seconds;

	return 0;
}

double service_get_restart_stability_threshold(const Service *service)
{
	if (service == NULL)
	{
		return -1.0;
	}

	return service->restart_stability_threshold_seconds;
}

int service_set_max_consecutive_restarts(Service *service, int max_restarts)
{
	if (service == NULL)
	{
		return -1;
	}

	service->max_consecutive_restarts = max_restarts;

	return 0;
}

int service_get_max_consecutive_restarts(const Service *service)
{
	if (service == NULL)
	{
		return -1;
	}

	return service->max_consecutive_restarts;
}

int service_set_stop_requested(Service *service, int stop_requested)
{
	if (service == NULL)
	{
		return -1;
	}

	service->stop_requested = stop_requested;

	return 0;
}

int service_get_stop_requested(const Service *service)
{
	if (service == NULL)
	{
		return 0;
	}

	return service->stop_requested;
}

int service_set_consecutive_restart_count(Service *service, int count)
{
	if (service == NULL)
	{
		return -1;
	}

	service->consecutive_restart_count = count;

	return 0;
}

int service_get_consecutive_restart_count(const Service *service)
{
	if (service == NULL)
	{
		return -1;
	}

	return service->consecutive_restart_count;
}

int service_set_permanently_failed(Service *service, int failed)
{
	if (service == NULL)
	{
		return -1;
	}

	service->permanently_failed = failed;

	return 0;
}

int service_has_failed_permanently(const Service *service)
{
	if (service == NULL)
	{
		return 0;
	}

	return service->permanently_failed;
}

int service_set_restart_pending(Service *service, int pending)
{
	if (service == NULL)
	{
		return -1;
	}

	service->restart_pending = pending;

	return 0;
}

int service_is_restart_pending(const Service *service)
{
	if (service == NULL)
	{
		return 0;
	}

	return service->restart_pending;
}

int service_set_last_start_time(Service *service, double monotonic_seconds)
{
	if (service == NULL)
	{
		return -1;
	}

	service->last_start_time = monotonic_seconds;

	return 0;
}

double service_get_last_start_time(const Service *service)
{
	if (service == NULL)
	{
		return -1.0;
	}

	return service->last_start_time;
}

int service_set_restart_allowed_at(Service *service, double monotonic_seconds)
{
	if (service == NULL)
	{
		return -1;
	}

	service->restart_allowed_at = monotonic_seconds;

	return 0;
}

double service_get_restart_allowed_at(const Service *service)
{
	if (service == NULL)
	{
		return -1.0;
	}

	return service->restart_allowed_at;
}

double service_monotonic_now(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);

	return ts.tv_sec + ts.tv_nsec / 1e9;
}

int service_add_dependency(Service *service, const char *dependency_name)
{
	if (service == NULL || dependency_name == NULL)
	{
		return -1;
	}

	if (strcmp(dependency_name, service->name) == 0)
	{
		embla_log_error("a service cannot depend on itself");
		return -1;
	}

	if (service->dependency_count >= SERVICE_MAX_DEPENDENCIES)
	{
		embla_log_error("service dependency capacity exceeded");
		return -1;
	}

	char *copy = embla_strdup(dependency_name);

	if (copy == NULL)
	{
		embla_log_error("failed to duplicate dependency name");
		return -1;
	}

	service->dependencies[service->dependency_count] = copy;
	service->dependency_count++;

	return 0;
}

int service_get_dependency_count(const Service *service)
{
	if (service == NULL)
	{
		return 0;
	}

	return service->dependency_count;
}

const char *service_get_dependency_name(const Service *service, size_t index)
{
	if (service == NULL || (int)index >= service->dependency_count)
	{
		return NULL;
	}

	return service->dependencies[index];
}
