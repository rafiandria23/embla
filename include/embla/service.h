#ifndef EMBLA_SERVICE_H
#define EMBLA_SERVICE_H

#include "embla/process.h"
#include "embla/process_config.h"

typedef struct Service Service;

Service *service_create(const char *name, ProcessConfig *config);

void service_destroy(Service *service);

const char *service_get_name(const Service *service);

const ProcessConfig *service_get_config(const Service *service);

typedef enum
{
	SERVICE_STOPPED,
	SERVICE_RUNNING,
} ServiceState;

int service_set_state(Service *service, ServiceState state);
ServiceState service_get_state(const Service *service);

int service_set_process(Service *service, Process *process);
Process *service_get_process(const Service *service);

int service_set_last_exit_code(Service *service, int exit_code);
int service_get_last_exit_code(const Service *service);

int service_set_last_term_signal(Service *service, int term_signal);
int service_get_last_term_signal(const Service *service);

typedef enum
{
	RESTART_NEVER,
	RESTART_ON_FAILURE,
	RESTART_ALWAYS,
} RestartPolicy;

int service_set_restart_policy(Service *service, RestartPolicy policy);
RestartPolicy service_get_restart_policy(const Service *service);

int service_set_restart_base_delay(Service *service, double seconds);
double service_get_restart_base_delay(const Service *service);

int service_set_restart_max_delay(Service *service, double seconds);
double service_get_restart_max_delay(const Service *service);

int service_set_restart_stability_threshold(Service *service, double seconds);
double service_get_restart_stability_threshold(const Service *service);

int service_set_max_consecutive_restarts(Service *service, int max_restarts);
int service_get_max_consecutive_restarts(const Service *service);

int service_set_stop_requested(Service *service, int stop_requested);
int service_get_stop_requested(const Service *service);

int service_set_consecutive_restart_count(Service *service, int count);
int service_get_consecutive_restart_count(const Service *service);

int service_set_permanently_failed(Service *service, int failed);
int service_has_failed_permanently(const Service *service);

int service_set_restart_pending(Service *service, int pending);
int service_is_restart_pending(const Service *service);

int service_set_last_start_time(Service *service, double monotonic_seconds);
double service_get_last_start_time(const Service *service);

int service_set_restart_allowed_at(Service *service, double monotonic_seconds);
double service_get_restart_allowed_at(const Service *service);

double service_monotonic_now(void);

int service_add_dependency(Service *service, const char *dependency_name);

int service_get_dependency_count(const Service *service);
const char *service_get_dependency_name(const Service *service, size_t index);

#endif
