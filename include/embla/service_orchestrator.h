#ifndef EMBLA_SERVICE_ORCHESTRATOR_H
#define EMBLA_SERVICE_ORCHESTRATOR_H

#include <stddef.h>

#include "embla/embla.h"
#include "embla/service_registry.h"

int service_registry_compute_start_order(
	const ServiceRegistry *registry,
	Service **out_order,
	size_t *out_count);

int service_registry_start_all(ServiceRegistry *registry, Embla *embla);

int service_registry_stop_all(
	ServiceRegistry *registry,
	Embla *embla,
	double timeout_seconds);

int service_registry_drain_one_event(Embla *embla);

int service_registry_supervise(
	ServiceRegistry *registry,
	Embla *embla,
	double stop_timeout_seconds);

#endif
