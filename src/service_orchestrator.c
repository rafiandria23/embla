#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>

#include "embla/log.h"
#include "embla/process_manager.h"
#include "embla/service_lifecycle.h"
#include "embla/service_orchestrator.h"

#define SERVICE_ORCHESTRATOR_KILL_WAIT_SECONDS 2.0

typedef enum
{
	SERVICE_SORT_WHITE,
	SERVICE_SORT_GRAY,
	SERVICE_SORT_BLACK,
} ServiceSortColor;

static int service_orchestrator_find_index(
	Service **all_services,
	size_t count,
	Service *target)
{
	for (size_t i = 0; i < count; i++)
	{
		if (all_services[i] == target)
		{
			return (int)i;
		}
	}

	return -1;
}

static int service_orchestrator_visit(
	const ServiceRegistry *registry,
	Service **all_services,
	ServiceSortColor *colors,
	size_t count,
	size_t index,
	Service **out_order,
	size_t *out_index)
{
	colors[index] = SERVICE_SORT_GRAY;

	Service *service = all_services[index];
	int dep_count = service_get_dependency_count(service);

	for (int i = 0; i < dep_count; i++)
	{
		const char *dep_name = service_get_dependency_name(service, (size_t)i);
		Service *dep = service_registry_get(registry, dep_name);

		if (dep == NULL)
		{
			embla_log_error("service depends on an unregistered service name");
			return -1;
		}

		int dep_index = service_orchestrator_find_index(all_services, count, dep);

		if (dep_index < 0)
		{
			embla_log_error("internal error: dependency not found in service list");
			return -1;
		}

		if (colors[dep_index] == SERVICE_SORT_GRAY)
		{
			embla_log_error("dependency cycle detected");
			return -1;
		}

		if (colors[dep_index] == SERVICE_SORT_WHITE)
		{
			if (service_orchestrator_visit(
					registry,
					all_services,
					colors,
					count,
					(size_t)dep_index,
					out_order,
					out_index) != 0)
			{
				return -1;
			}
		}
	}

	colors[index] = SERVICE_SORT_BLACK;
	out_order[*out_index] = service;
	(*out_index)++;

	return 0;
}

int service_registry_compute_start_order(
	const ServiceRegistry *registry,
	Service **out_order,
	size_t *out_count)
{
	if (
		registry == NULL ||
		out_order == NULL ||
		out_count == NULL)
	{
		return -1;
	}

	size_t count = service_registry_count(registry);

	if (count == 0)
	{
		*out_count = 0;
		return 0;
	}

	Service **all_services = malloc(count * sizeof(*all_services));

	if (all_services == NULL)
	{
		embla_log_error("failed to allocate service list for ordering");
		return -1;
	}

	ServiceSortColor *colors = malloc(count * sizeof(*colors));

	if (colors == NULL)
	{
		embla_log_error("failed to allocate color tracking for ordering");

		free(all_services);

		return -1;
	}

	for (size_t i = 0; i < count; i++)
	{
		all_services[i] = service_registry_get_at(registry, i);
		colors[i] = SERVICE_SORT_WHITE;
	}

	size_t out_index = 0;
	int result = 0;

	for (size_t i = 0; i < count; i++)
	{
		if (colors[i] == SERVICE_SORT_WHITE)
		{
			if (service_orchestrator_visit(
					registry,
					all_services,
					colors,
					count,
					i,
					out_order,
					&out_index) != 0)
			{
				result = -1;
				break;
			}
		}
	}

	free(colors);
	free(all_services);

	if (result != 0)
	{
		return -1;
	}

	*out_count = out_index;

	return 0;
}

int service_registry_start_all(ServiceRegistry *registry, Embla *embla)
{
	if (registry == NULL || embla == NULL)
	{
		return -1;
	}

	size_t count = service_registry_count(registry);

	if (count == 0)
	{
		return 0;
	}

	Service **order = malloc(count * sizeof(*order));

	if (order == NULL)
	{
		embla_log_error("failed to allocate start order buffer");
		return -1;
	}

	size_t order_count;

	if (service_registry_compute_start_order(
			registry,
			order,
			&order_count) != 0)
	{
		free(order);
		return -1;
	}

	int started = 0;

	for (size_t i = 0; i < order_count; i++)
	{
		Service *service = order[i];

		int all_deps_running = 1;
		int dep_count = service_get_dependency_count(service);

		for (int j = 0; j < dep_count; j++)
		{
			const char *dep_name = service_get_dependency_name(
				service,
				(size_t)j);
			Service *dep = service_registry_get(registry, dep_name);

			if (
				dep == NULL ||
				service_get_state(dep) != SERVICE_RUNNING)
			{
				all_deps_running = 0;
				break;
			}
		}

		if (!all_deps_running)
		{
			embla_log_error("skipping service start due to a failed dependency");
			continue;
		}

		if (service_start(service, embla) == 0)
		{
			started++;
		}
		else
		{
			embla_log_error("service failed to start");
		}
	}

	free(order);

	return started;
}

static int service_orchestrator_wait_for_termination(
	Service *service,
	Embla *embla,
	double deadline)
{
	while (service_monotonic_now() < deadline)
	{
		int reap_result = service_reap(service, embla);

		if (reap_result == 1)
		{
			return 1;
		}

		if (reap_result < 0)
		{
			return -1;
		}

		HostProcessId host_id;
		int wait_status;
		struct rusage usage;

		int poll_result = executor_poll_any(
			embla_executor(embla),
			&host_id,
			&wait_status,
			&usage);

		if (poll_result < 0)
		{
			return -1;
		}

		if (poll_result == 0)
		{
			struct timespec tiny = {
				.tv_sec = 0,
				.tv_nsec = 5000000};

			nanosleep(&tiny, NULL);

			continue;
		}

		Process *event_process = process_manager_get_by_host_id(
			embla_process_manager(embla),
			host_id);

		if (event_process == NULL)
		{
			continue;
		}

		if (WIFEXITED(wait_status))
		{
			process_set_exit_code(
				event_process,
				WEXITSTATUS(wait_status));
		}
		else if (WIFSIGNALED(wait_status))
		{
			process_set_term_signal(
				event_process,
				WTERMSIG(wait_status));
		}
		else
		{
			continue;
		}

		process_transition(event_process, PROCESS_TERMINATED);
		executor_apply_rusage(event_process, &usage);
	}

	return 0;
}

int service_registry_stop_all(
	ServiceRegistry *registry,
	Embla *embla,
	double timeout_seconds)
{
	if (registry == NULL || embla == NULL)
	{
		return -1;
	}

	size_t count = service_registry_count(registry);

	if (count == 0)
	{
		return 0;
	}

	Service **order = malloc(count * sizeof(*order));

	if (order == NULL)
	{
		embla_log_error("failed to allocate stop order buffer");
		return -1;
	}

	size_t order_count;

	if (service_registry_compute_start_order(
			registry,
			order,
			&order_count) != 0)
	{
		free(order);
		return -1;
	}

	int stopped = 0;

	for (size_t i = order_count; i > 0; i--)
	{
		Service *service = order[i - 1];

		if (service_get_state(service) != SERVICE_RUNNING)
		{
			continue;
		}

		if (service_stop(service, embla) != 0)
		{
			embla_log_error("failed to send stop signal to service");
			continue;
		}

		double deadline = service_monotonic_now() + timeout_seconds;
		int terminated = service_orchestrator_wait_for_termination(
			service,
			embla,
			deadline);

		if (terminated != 1)
		{
			embla_log_error("service did not stop in time, escalating to SIGKILL");

			if (service_kill(service, embla) == 0)
			{
				double kill_deadline =
					service_monotonic_now() + SERVICE_ORCHESTRATOR_KILL_WAIT_SECONDS;

				terminated = service_orchestrator_wait_for_termination(
					service,
					embla,
					kill_deadline);
			}
		}

		if (terminated == 1)
		{
			stopped++;
		}
		else
		{
			embla_log_error(
				"service could not be stopped even after SIGKILL "
				"escalation");
		}
	}

	free(order);

	return stopped;
}
