#ifndef EMBLA_SERVICE_REGISTRY_H
#define EMBLA_SERVICE_REGISTRY_H

#include <stddef.h>

#include "embla/service.h"

typedef struct ServiceRegistry ServiceRegistry;

ServiceRegistry *service_registry_create(void);

void service_registry_destroy(ServiceRegistry *registry);

int service_registry_register(
	ServiceRegistry *registry,
	Service *service);

Service *service_registry_get(
	const ServiceRegistry *registry,
	const char *name);

size_t service_registry_count(const ServiceRegistry *registry);

int service_registry_unregister(
	ServiceRegistry *registry,
	const char *name);

#endif
