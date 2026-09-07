#define _POSIX_C_SOURCE 200809L

#include "embla/embla.h"
#include "embla/process_config.h"
#include "embla/service.h"
#include "embla/service_orchestrator.h"
#include "embla/service_registry.h"

static Service *make_example_service(void)
{
	char *argv[] = {
		"sh",
		"-c",
		"i=0; while true; do "
		"i=$((i + 1)); "
		"echo \"heartbeat $i\"; "
		"sleep 2; "
		"done",
		NULL};

	ProcessConfig *config = process_config_create("/bin/sh", argv);

	if (config == NULL)
	{
		return NULL;
	}

	Service *service = service_create("heartbeat", config);

	if (service == NULL)
	{
		process_config_destroy(config);
		return NULL;
	}

	service_set_restart_policy(service, RESTART_ALWAYS);

	return service;
}

int main(void)
{
	embla_install_shutdown_handlers();

	Embla *embla = embla_create();

	if (embla == NULL)
	{
		return 1;
	}

	ServiceRegistry *registry = service_registry_create();

	if (registry == NULL)
	{
		embla_destroy(embla);
		return 1;
	}

	Service *heartbeat = make_example_service();

	if (heartbeat == NULL || service_registry_register(registry, heartbeat) != 0)
	{
		service_destroy(heartbeat);
		service_registry_destroy(registry);
		embla_destroy(embla);

		return 1;
	}

	int stopped = service_registry_supervise(registry, embla, 10.0);

	service_registry_destroy(registry);
	embla_destroy(embla);

	return stopped < 0;
}
