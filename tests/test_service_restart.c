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
#include "embla/service_restart.h"

#define CHECK(cond, msg)                                                    \
	do                                                                      \
	{                                                                       \
		if (!(cond))                                                        \
		{                                                                   \
			fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
			return -1;                                                      \
		}                                                                   \
	} while (0)

static int drain_until_terminated(Embla *embla, Service *service)
{
	Process *process = service_get_process(service);

	if (process == NULL)
	{
		return -1;
	}

	while (process_get_state(process) != PROCESS_TERMINATED)
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

		Process *event_process = process_manager_get_by_host_id(
			embla_process_manager(embla),
			host_id);

		if (event_process == NULL)
		{
			return -1;
		}

		if (WIFEXITED(wait_status))
		{
			if (process_set_exit_code(
					event_process,
					WEXITSTATUS(wait_status)) != 0)
			{
				return -1;
			}
		}
		else if (WIFSIGNALED(wait_status))
		{
			if (process_set_term_signal(
					event_process,
					WTERMSIG(wait_status)) != 0)
			{
				return -1;
			}
		}
		else
		{
			continue;
		}

		if (process_transition(event_process, PROCESS_TERMINATED) != 0)
		{
			return -1;
		}

		if (executor_apply_rusage(event_process, &usage) != 0)
		{
			return -1;
		}
	}

	return 0;
}

static int wait_for_tick_result(
	Service *service,
	Embla *embla,
	ServiceTickResult expected,
	double timeout_seconds)
{
	double deadline = service_monotonic_now() + timeout_seconds;

	for (;;)
	{
		ServiceTickResult result = service_tick(service, embla);

		if (result == expected)
		{
			return 0;
		}

		if (result != SERVICE_TICK_WAITING)
		{
			fprintf(
				stderr,
				"unexpected tick result %d, wanted %d\n",
				(int)result,
				(int)expected);

			return -1;
		}

		if (service_monotonic_now() > deadline)
		{
			fprintf(stderr, "timed out waiting for tick result\n");
			return -1;
		}

		struct timespec tiny = {
			.tv_sec = 0,
			.tv_nsec = 5000000};

		nanosleep(&tiny, NULL);
	}
}

static ProcessConfig *make_exit_config(int code)
{
	char argv0[] = "sh";
	char argv1[] = "-c";
	char command[32];

	snprintf(command, sizeof(command), "exit %d", code);

	char *argv[] = {argv0, argv1, command, NULL};

	return process_config_create("/bin/sh", argv);
}

static ProcessConfig *make_sleep_config(void)
{
	char *argv[] = {"sleep", "30", NULL};

	return process_config_create("/bin/sleep", argv);
}

static int test_restart_never(void)
{
	Embla *embla = embla_create();
	Service *service = service_create("never", make_exit_config(1));

	CHECK(embla != NULL && service != NULL, "fixtures should allocate");
	CHECK(
		service_get_restart_policy(service) == RESTART_NEVER,
		"RESTART_NEVER should be the default for a new service");

	CHECK(service_start(service, embla) == 0, "start should succeed");
	CHECK(drain_until_terminated(embla, service) == 0, "drain should succeed");

	ServiceTickResult result = service_tick(service, embla);

	CHECK(
		result == SERVICE_TICK_REAPED_NO_RESTART,
		"a failing exit under RESTART_NEVER must not schedule a "
		"restart");
	CHECK(
		service_get_state(service) == SERVICE_STOPPED,
		"the service should be SERVICE_STOPPED");
	CHECK(
		!service_is_restart_pending(service),
		"no restart should be pending");

	service_destroy(service);
	embla_destroy(embla);

	return 0;
}

static int test_restart_on_failure_distinguishes_clean_exit(void)
{
	Embla *embla = embla_create();
	Service *failing = service_create("failing", make_exit_config(1));
	Service *clean = service_create("clean", make_exit_config(0));

	CHECK(
		embla != NULL && failing != NULL && clean != NULL,
		"fixtures should allocate");

	service_set_restart_policy(failing, RESTART_ON_FAILURE);
	service_set_restart_base_delay(failing, 0.05);
	service_set_restart_policy(clean, RESTART_ON_FAILURE);

	CHECK(service_start(failing, embla) == 0, "starting failing should succeed");
	CHECK(drain_until_terminated(embla, failing) == 0, "drain should succeed");
	CHECK(
		service_tick(failing, embla) == SERVICE_TICK_REAPED_RESTART_SCHEDULED,
		"a nonzero exit under RESTART_ON_FAILURE must schedule a "
		"restart");
	CHECK(
		wait_for_tick_result(
			failing,
			embla,
			SERVICE_TICK_RESTARTED,
			2.0) == 0,
		"the scheduled restart should eventually happen");
	CHECK(
		service_get_state(failing) == SERVICE_RUNNING,
		"failing should be running again");

	CHECK(service_start(clean, embla) == 0, "starting clean should succeed");
	CHECK(drain_until_terminated(embla, clean) == 0, "drain should succeed");
	CHECK(
		service_tick(clean, embla) == SERVICE_TICK_REAPED_NO_RESTART,
		"a CLEAN exit under RESTART_ON_FAILURE must NOT schedule a "
		"restart -- this is the entire distinction ON_FAILURE makes "
		"versus ALWAYS");
	CHECK(
		service_get_state(clean) == SERVICE_STOPPED,
		"clean should remain SERVICE_STOPPED");

	service_stop(failing, embla);
	drain_until_terminated(embla, failing);
	service_tick(failing, embla);

	service_destroy(failing);
	service_destroy(clean);
	embla_destroy(embla);

	return 0;
}

static int test_restart_always_restarts_on_clean_exit(void)
{
	Embla *embla = embla_create();
	Service *service = service_create("always", make_exit_config(0));

	CHECK(embla != NULL && service != NULL, "fixtures should allocate");

	service_set_restart_policy(service, RESTART_ALWAYS);
	service_set_restart_base_delay(service, 0.05);

	CHECK(service_start(service, embla) == 0, "start should succeed");
	CHECK(drain_until_terminated(embla, service) == 0, "drain should succeed");
	CHECK(
		service_tick(service, embla) == SERVICE_TICK_REAPED_RESTART_SCHEDULED,
		"even a CLEAN exit under RESTART_ALWAYS must schedule a "
		"restart");
	CHECK(
		wait_for_tick_result(
			service,
			embla,
			SERVICE_TICK_RESTARTED,
			2.0) == 0,
		"the scheduled restart should eventually happen");

	service_stop(service, embla);
	drain_until_terminated(embla, service);
	service_tick(service, embla);

	service_destroy(service);
	embla_destroy(embla);

	return 0;
}

static int test_explicit_stop_suppresses_restart_always(void)
{
	Embla *embla = embla_create();
	Service *service = service_create("stoppable", make_sleep_config());

	CHECK(embla != NULL && service != NULL, "fixtures should allocate");

	service_set_restart_policy(service, RESTART_ALWAYS);
	service_set_restart_base_delay(service, 0.05);

	CHECK(service_start(service, embla) == 0, "start should succeed");
	CHECK(service_stop(service, embla) == 0, "stop should succeed");
	CHECK(drain_until_terminated(embla, service) == 0, "drain should succeed");

	ServiceTickResult result = service_tick(service, embla);

	CHECK(
		result == SERVICE_TICK_REAPED_NO_RESTART,
		"an explicit stop must suppress restart even under "
		"RESTART_ALWAYS -- this is the one thing every real "
		"process supervisor agrees on");
	CHECK(
		!service_is_restart_pending(service),
		"no restart should be pending after an explicit stop");
	CHECK(
		service_get_state(service) == SERVICE_STOPPED,
		"the service should be SERVICE_STOPPED");

	service_destroy(service);
	embla_destroy(embla);

	return 0;
}

static int test_backoff_delay_doubles(void)
{
	Embla *embla = embla_create();
	Service *service = service_create("crasher", make_exit_config(1));

	CHECK(embla != NULL && service != NULL, "fixtures should allocate");

	service_set_restart_policy(service, RESTART_ALWAYS);
	service_set_restart_base_delay(service, 0.05);
	service_set_restart_max_delay(service, 10.0);
	service_set_restart_stability_threshold(service, 1000.0);
	service_set_max_consecutive_restarts(service, 10);

	CHECK(service_start(service, embla) == 0, "start should succeed");

	double expected_delays[] = {0.05, 0.10, 0.20};

	for (int cycle = 0; cycle < 3; cycle++)
	{
		CHECK(drain_until_terminated(embla, service) == 0, "drain should succeed");

		double before_schedule = service_monotonic_now();

		CHECK(
			service_tick(service, embla) == SERVICE_TICK_REAPED_RESTART_SCHEDULED,
			"each crash should schedule a restart");
		CHECK(
			service_get_consecutive_restart_count(service) == cycle + 1,
			"the consecutive restart count should increment by "
			"exactly 1 each cycle");

		double scheduled_delay =
			service_get_restart_allowed_at(service) - before_schedule;
		double expected = expected_delays[cycle];

		CHECK(
			scheduled_delay > expected * 0.5 &&
				scheduled_delay < expected * 2.0,
			"the SCHEDULED delay should match base_delay * "
			"2^(count-1) for this cycle, within tolerance -- this "
			"is the actual doubling math, checked directly rather "
			"than only inferred from how long we end up waiting");

		CHECK(
			service_tick(service, embla) == SERVICE_TICK_WAITING,
			"ticking immediately after scheduling must report "
			"WAITING, not restart early");

		CHECK(
			wait_for_tick_result(
				service,
				embla,
				SERVICE_TICK_RESTARTED,
				5.0) == 0,
			"the scheduled restart should eventually happen");

		double actual_elapsed = service_monotonic_now() - before_schedule;

		CHECK(
			actual_elapsed >= expected * 0.8,
			"the REAL elapsed wall-clock time before the restart "
			"actually happened must be at least close to the "
			"expected delay -- proving service_tick() genuinely "
			"waits, not just that the math is computed correctly");
	}

	service_stop(service, embla);
	drain_until_terminated(embla, service);
	service_tick(service, embla);

	service_destroy(service);
	embla_destroy(embla);

	return 0;
}

static int test_stability_threshold_resets_count(void)
{
	Embla *embla = embla_create();
	Service *service = service_create("flapper", make_exit_config(1));

	CHECK(embla != NULL && service != NULL, "fixtures should allocate");

	service_set_restart_policy(service, RESTART_ALWAYS);
	service_set_restart_base_delay(service, 0.02);
	service_set_restart_stability_threshold(service, 5.0);
	service_set_max_consecutive_restarts(service, 10);

	CHECK(service_start(service, embla) == 0, "start should succeed");

	CHECK(drain_until_terminated(embla, service) == 0, "drain should succeed");
	CHECK(
		service_tick(service, embla) == SERVICE_TICK_REAPED_RESTART_SCHEDULED,
		"the first crash should schedule a restart");
	CHECK(
		service_get_consecutive_restart_count(service) == 1,
		"count should be 1 after the first crash");
	CHECK(
		wait_for_tick_result(
			service,
			embla,
			SERVICE_TICK_RESTARTED,
			2.0) == 0,
		"the scheduled restart should happen");

	CHECK(
		service_set_last_start_time(
			service, service_monotonic_now() - 10.0) == 0,
		"backdating last_start_time should succeed");

	CHECK(drain_until_terminated(embla, service) == 0, "drain should succeed");
	CHECK(
		service_tick(service, embla) == SERVICE_TICK_REAPED_RESTART_SCHEDULED,
		"the second crash should also schedule a restart");
	CHECK(
		service_get_consecutive_restart_count(service) == 1,
		"count should be back to 1, not 2 -- proving the stability "
		"threshold genuinely reset it; without the reset this "
		"would read 2");

	CHECK(
		wait_for_tick_result(
			service,
			embla,
			SERVICE_TICK_RESTARTED,
			2.0) == 0,
		"the scheduled restart should happen");

	service_stop(service, embla);
	drain_until_terminated(embla, service);
	service_tick(service, embla);

	service_destroy(service);
	embla_destroy(embla);

	return 0;
}

static int test_gives_up_after_max_consecutive_restarts(void)
{
	Embla *embla = embla_create();
	Service *service = service_create("doomed", make_exit_config(1));

	CHECK(embla != NULL && service != NULL, "fixtures should allocate");

	service_set_restart_policy(service, RESTART_ALWAYS);
	service_set_restart_base_delay(service, 0.02);
	service_set_restart_stability_threshold(service, 1000.0);
	service_set_max_consecutive_restarts(service, 3);

	CHECK(service_start(service, embla) == 0, "start should succeed");

	for (int cycle = 0; cycle < 3; cycle++)
	{
		CHECK(drain_until_terminated(embla, service) == 0, "drain should succeed");
		CHECK(
			service_tick(service, embla) == SERVICE_TICK_REAPED_RESTART_SCHEDULED,
			"cycles within the limit should schedule a restart");
		CHECK(
			!service_has_failed_permanently(service),
			"should not be marked permanently failed yet");
		CHECK(
			wait_for_tick_result(
				service,
				embla,
				SERVICE_TICK_RESTARTED,
				2.0) == 0,
			"the scheduled restart should happen");
	}

	CHECK(drain_until_terminated(embla, service) == 0, "drain should succeed");

	ServiceTickResult result = service_tick(service, embla);

	CHECK(
		result == SERVICE_TICK_GAVE_UP,
		"exceeding max_consecutive_restarts must give up rather "
		"than schedule yet another restart");
	CHECK(
		service_has_failed_permanently(service),
		"the service should now be marked permanently failed");
	CHECK(
		!service_is_restart_pending(service),
		"no restart should be pending after giving up");
	CHECK(
		service_get_state(service) == SERVICE_STOPPED,
		"the service should be SERVICE_STOPPED");

	CHECK(
		service_tick(service, embla) == SERVICE_TICK_NOTHING,
		"ticking again after giving up must do nothing "
		"automatically -- the service is stopped, with no process "
		"and no restart pending");

	CHECK(
		service_start(service, embla) == 0,
		"a caller should still be able to manually restart a "
		"permanently-failed service");
	CHECK(
		!service_has_failed_permanently(service),
		"a successful manual restart should clear the "
		"permanently-failed flag");

	service_stop(service, embla);
	drain_until_terminated(embla, service);
	service_tick(service, embla);

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
		{"restart_never", test_restart_never},
		{
			"restart_on_failure_distinguishes_clean_exit",
			test_restart_on_failure_distinguishes_clean_exit,
		},
		{
			"restart_always_restarts_on_clean_exit",
			test_restart_always_restarts_on_clean_exit,
		},
		{
			"explicit_stop_suppresses_restart_always",
			test_explicit_stop_suppresses_restart_always,
		},
		{"backoff_delay_doubles", test_backoff_delay_doubles},
		{
			"stability_threshold_resets_count",
			test_stability_threshold_resets_count,
		},
		{
			"gives_up_after_max_consecutive_restarts",
			test_gives_up_after_max_consecutive_restarts,
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

	printf("all %zu restart policy tests passed\n", count);

	return EXIT_SUCCESS;
}
