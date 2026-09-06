#ifndef EMBLA_SERVICE_H
#define EMBLA_SERVICE_H

#include "embla/process.h"
#include "embla/process_config.h"

typedef enum
{
	SERVICE_STOPPED,
	SERVICE_RUNNING,
} ServiceState;

typedef struct Service Service;

Service *service_create(const char *name, ProcessConfig *config);

void service_destroy(Service *service);

const char *service_get_name(const Service *service);

const ProcessConfig *service_get_config(const Service *service);

int service_set_state(Service *service, ServiceState state);
ServiceState service_get_state(const Service *service);

int service_set_process(Service *service, Process *process);
Process *service_get_process(const Service *service);

int service_set_last_exit_code(Service *service, int exit_code);
int service_get_last_exit_code(const Service *service);

int service_set_last_term_signal(Service *service, int term_signal);
int service_get_last_term_signal(const Service *service);

#endif
