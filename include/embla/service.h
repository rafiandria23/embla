#ifndef EMBLA_SERVICE_H
#define EMBLA_SERVICE_H

#include "embla/process_config.h"

typedef struct Service Service;

Service *service_create(
	const char *name,
	ProcessConfig *config);

void service_destroy(Service *service);

const char *service_get_name(const Service *service);

const ProcessConfig *service_get_config(const Service *service);

#endif
