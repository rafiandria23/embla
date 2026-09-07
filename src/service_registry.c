#include <stdlib.h>
#include <string.h>

#include "embla/log.h"
#include "embla/service_registry.h"

#define SERVICE_REGISTRY_INITIAL_CAPACITY 16
#define SERVICE_REGISTRY_NOT_FOUND ((size_t)-1)

struct ServiceRegistry
{
	Service **services;

	size_t count;
	size_t capacity;
};

ServiceRegistry *service_registry_create(void)
{
	ServiceRegistry *registry = malloc(sizeof(*registry));

	if (registry == NULL)
	{
		embla_log_error("failed to allocate service registry");
		return NULL;
	}

	registry->services = calloc(
		SERVICE_REGISTRY_INITIAL_CAPACITY,
		sizeof(*registry->services));

	if (registry->services == NULL)
	{
		embla_log_error("failed to allocate service table");

		free(registry);

		return NULL;
	}

	registry->count = 0;
	registry->capacity = SERVICE_REGISTRY_INITIAL_CAPACITY;

	return registry;
}

void service_registry_destroy(ServiceRegistry *registry)
{
	if (registry == NULL)
	{
		return;
	}

	for (size_t i = 0; i < registry->count; i++)
	{
		service_destroy(registry->services[i]);
	}

	free(registry->services);
	free(registry);
}

static size_t service_registry_find_index(const ServiceRegistry *registry, const char *name)
{
	for (size_t i = 0; i < registry->count; i++)
	{
		if (strcmp(
				service_get_name(registry->services[i]),
				name) == 0)
		{
			return i;
		}
	}

	return SERVICE_REGISTRY_NOT_FOUND;
}

int service_registry_register(ServiceRegistry *registry, Service *service)
{
	if (registry == NULL || service == NULL)
	{
		return -1;
	}

	if (registry->count >= registry->capacity)
	{
		embla_log_error("service registry is full");
		return -1;
	}

	if (
		service_registry_find_index(
			registry,
			service_get_name(service)) != SERVICE_REGISTRY_NOT_FOUND)
	{
		embla_log_error("a service with this name is already registered");
		return -1;
	}

	registry->services[registry->count] = service;
	registry->count++;

	return 0;
}

Service *service_registry_get(const ServiceRegistry *registry, const char *name)
{
	if (registry == NULL || name == NULL)
	{
		return NULL;
	}

	size_t index = service_registry_find_index(registry, name);

	if (index == SERVICE_REGISTRY_NOT_FOUND)
	{
		return NULL;
	}

	return registry->services[index];
}

size_t service_registry_count(const ServiceRegistry *registry)
{
	if (registry == NULL)
	{
		return 0;
	}

	return registry->count;
}

Service *service_registry_get_at(const ServiceRegistry *registry, size_t index)
{
	if (registry == NULL || index >= registry->count)
	{
		return NULL;
	}

	return registry->services[index];
}

int service_registry_unregister(ServiceRegistry *registry, const char *name)
{
	if (registry == NULL || name == NULL)
	{
		return -1;
	}

	size_t index = service_registry_find_index(
		registry,
		name);

	if (index == SERVICE_REGISTRY_NOT_FOUND)
	{
		return -1;
	}

	service_destroy(registry->services[index]);

	size_t last = registry->count - 1;

	registry->services[index] = registry->services[last];
	registry->services[last] = NULL;

	registry->count--;

	return 0;
}
