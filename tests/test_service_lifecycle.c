
#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "embla/embla.h"
#include "embla/process_config.h"
#include "embla/process_manager.h"
#include "embla/service.h"
#include "embla/service_lifecycle.h"

#define CHECK(cond, msg)                                                    \
	do                                                                      \
	{                                                                       \
		if (!(cond))                                                        \
		{                                                                   \
			fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
			return -1;                                                      \
		}                                                                   \
	} while (0)

static int drain_one_termination(Embla *embla)
{
	for (;;)
	{
		HostProcessId host_id;
		int wait_status;
		struct rusage usage;

		int result = executor_poll_any(
			embla_executor(embla),
			&host_id,
			&wait_status,
			&usage);

		if (result < 0)
		{
			return -1;
		}

		if (result == 0)
		{
			struct timespec delay = {
				.tv_sec = 0,
				.tv_nsec = 1000000};

			nanosleep(&delay, NULL);

			continue;
		}

		Process *process = process_manager_get_by_host_id(
			embla_process_manager(embla),
			host_id);

		if (process == NULL)
		{
			return -1;
		}

		if (WIFEXITED(wait_status))
		{
			if (process_set_exit_code(
					process,
					WEXITSTATUS(wait_status)) != 0)
			{
				return -1;
			}
		}
		else if (WIFSIGNALED(wait_status))
		{
			if (process_set_term_signal(
					process,
					WTERMSIG(wait_status)) != 0)
			{
				return -1;
			}
		}
		else
		{
			continue;
		}

		if (process_transition(process, PROCESS_TERMINATED) != 0)
		{
			return -1;
		}

		if (executor_apply_rusage(process, &usage) != 0)
		{
			return -1;
		}

		return 0;
	}
}

static ProcessConfig *make_sleep_config(void)
{
	char *argv[] = {"sleep", "30", NULL};

	return process_config_create("/bin/sleep", argv);
}

static int test_full_lifecycle_and_restart(void)
{
	Embla *embla = embla_create();

	CHECK(embla != NULL, "embla_create should succeed");

	Service *service = service_create("worker", make_sleep_config());

	CHECK(service != NULL, "service_create should succeed");
	CHECK(
		service_get_state(service) == SERVICE_STOPPED,
		"a freshly created service should start SERVICE_STOPPED");

	CHECK(
		service_start(service, embla) == 0,
		"starting the service should succeed");
	CHECK(
		service_get_state(service) == SERVICE_RUNNING,
		"the service should now be SERVICE_RUNNING");
	CHECK(
		service_get_process(service) != NULL,
		"the service should now be tracking a live process");
	CHECK(
		service_start(service, embla) != 0,
		"starting an ALREADY-running service must be rejected -- "
		"one live process per service at a time");

	CHECK(
		service_reap(service, embla) == 0,
		"reaping a service whose process hasn't terminated yet "
		"should be a no-op, returning 0");
	CHECK(
		service_stop(service, embla) == 0,
		"stopping the service should succeed");
	CHECK(
		service_get_state(service) == SERVICE_RUNNING,
		"the service must STILL read as SERVICE_RUNNING "
		"immediately after stop() -- stopping is event-driven, "
		"not synchronous; the state only changes once termination "
		"is actually observed via service_reap()");

	CHECK(
		drain_one_termination(embla) == 0,
		"draining the termination event should succeed");
	CHECK(
		service_reap(service, embla) == 1,
		"reaping should now detect the termination and return 1");
	CHECK(
		service_get_state(service) == SERVICE_STOPPED,
		"the service should be SERVICE_STOPPED after being reaped");
	CHECK(
		service_get_process(service) == NULL,
		"the service should no longer be tracking any process");
	CHECK(
		service_get_last_term_signal(service) == SIGTERM,
		"the captured term signal should be SIGTERM, matching "
		"what service_stop() actually sent");
	CHECK(
		process_manager_count(embla_process_manager(embla)) == 0,
		"the process should have been genuinely reaped (destroyed), "
		"not just marked stopped");

	CHECK(
		service_start(service, embla) == 0,
		"restarting the same service after a full stop+reap "
		"cycle should succeed");
	CHECK(
		service_get_state(service) == SERVICE_RUNNING,
		"the restarted service should be SERVICE_RUNNING again");

	Process *second_process = service_get_process(service);

	CHECK(
		second_process != NULL,
		"the restarted service should be tracking a NEW live "
		"process");

	CHECK(
		service_stop(service, embla) == 0,
		"stopping the restarted service should succeed");
	CHECK(
		drain_one_termination(embla) == 0,
		"draining the second termination should succeed");
	CHECK(
		service_reap(service, embla) == 1,
		"reaping the second instance should succeed");

	service_destroy(service);
	embla_destroy(embla);

	return 0;
}

static int test_stop_when_not_running_fails(void)
{
	Embla *embla = embla_create();

	CHECK(embla != NULL, "embla_create should succeed");

	Service *service = service_create("idle", make_sleep_config());

	CHECK(service != NULL, "service_create should succeed");
	CHECK(
		service_stop(service, embla) != 0,
		"stopping a service that was never started must fail");

	service_destroy(service);
	embla_destroy(embla);

	return 0;
}

int main(void)
{
	struct
	{
		const char *name;
		int (*fn)(void);
	} tests[] = {
		{
			"full_lifecycle_and_restart",
			test_full_lifecycle_and_restart,
		},
		{
			"stop_when_not_running_fails",
			test_stop_when_not_running_fails,
		},
	};

	size_t count = sizeof(tests) / sizeof(tests[0]);
	int failures = 0;

	for (size_t i = 0; i < count; i++)
	{
		printf("-- %s\n", tests[i].name);

		if (tests[i].fn() != 0)
		{
			failures++;
		}
	}

	if (failures > 0)
	{
		fprintf(stderr, "%d test(s) failed\n", failures);
		return EXIT_FAILURE;
	}

	printf("all %zu service lifecycle tests passed\n", count);

	return EXIT_SUCCESS;
}
