#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "embla/embla.h"
#include "embla/process_config.h"
#include "embla/service.h"
#include "embla/service_orchestrator.h"
#include "embla/service_registry.h"

#define CHECK(cond, msg)                                                    \
	do                                                                      \
	{                                                                       \
		if (!(cond))                                                        \
		{                                                                   \
			fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
			return -1;                                                      \
		}                                                                   \
	} while (0)

static ProcessConfig *make_sleep_config(void)
{
	char *argv[] = {"sleep", "30", NULL};

	return process_config_create("/bin/sleep", argv);
}

static ProcessConfig *make_crash_config(void)
{
	char *argv[] = {"sh", "-c", "exit 1", NULL};

	return process_config_create("/bin/sh", argv);
}

static int test_shutdown_starts_unrequested(void)
{
	embla_clear_shutdown_request();

	CHECK(
		embla_shutdown_was_requested() == 0,
		"the shutdown flag should read false after being cleared");

	return 0;
}

static int test_request_shutdown_programmatic(void)
{
	embla_clear_shutdown_request();

	CHECK(
		embla_shutdown_was_requested() == 0,
		"the flag should start false for this test");

	embla_request_shutdown();

	CHECK(
		embla_shutdown_was_requested() == 1,
		"embla_request_shutdown() should set the flag without "
		"needing any real OS signal");

	embla_clear_shutdown_request();

	CHECK(
		embla_shutdown_was_requested() == 0,
		"clearing the request should reset the flag back to false");

	return 0;
}

static int test_real_sigterm_sets_flag(void)
{
	embla_clear_shutdown_request();
	embla_install_shutdown_handlers();

	CHECK(
		embla_shutdown_was_requested() == 0,
		"the flag should start false before the signal is sent");

	raise(SIGTERM);

	CHECK(
		embla_shutdown_was_requested() == 1,
		"a real SIGTERM, delivered synchronously via raise(), "
		"should have set the flag through the installed handler -- "
		"proving the handler is genuinely wired up, not merely "
		"that the flag mechanism itself works");

	embla_clear_shutdown_request();

	return 0;
}

static int test_drain_handles_untracked_grandchild_gracefully(void)
{
	Embla *embla = embla_create();

	CHECK(embla != NULL, "fixture should allocate");

	char *argv[] = {"sh", "-c", "(sleep 1 &) ; exit 0", NULL};
	ProcessConfig *config = process_config_create("/bin/sh", argv);

	CHECK(config != NULL, "config create should succeed");

	Process *forker = embla_spawn(embla, "forker", config);

	CHECK(forker != NULL, "spawn should succeed");

	double deadline = service_monotonic_now() + 3.0;
	int saw_forker_terminate = 0;

	while (service_monotonic_now() < deadline)
	{
		int result = service_registry_drain_one_event(embla);

		CHECK(
			result >= 0,
			"draining must never fail, including for the "
			"untracked grandchild's own eventual termination "
			"event -- this is exactly what would have made "
			"embla_handle_child_event()'s PRE-2.13.1 behavior "
			"fatal");

		if (process_get_state(forker) == PROCESS_TERMINATED)
		{
			saw_forker_terminate = 1;
		}

		if (result == 0)
		{
			struct timespec tiny = {.tv_sec = 0, .tv_nsec = 5000000};

			nanosleep(&tiny, NULL);
		}
	}

	CHECK(
		saw_forker_terminate,
		"forker's own termination should have been observed "
		"correctly, despite the untracked grandchild event "
		"happening in between");

	embla_reap_process(embla, process_get_id(forker));
	process_config_destroy(config);
	embla_destroy(embla);

	return 0;
}

static volatile sig_atomic_t alarm_fired = 0;

static void test_alarm_handler(int signum)
{
	(void)signum;
	alarm_fired = 1;
	embla_request_shutdown();
}

static int install_bounding_alarm(long usec)
{
	struct sigaction action;

	action.sa_handler = test_alarm_handler;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;

	if (sigaction(SIGALRM, &action, NULL) != 0)
	{
		return -1;
	}

	struct itimerval timer;

	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = usec;
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 0;

	return setitimer(ITIMER_REAL, &timer, NULL);
}

static int test_supervise_runs_then_stops_on_request(void)
{
	embla_clear_shutdown_request();
	alarm_fired = 0;

	Embla *embla = embla_create();
	ServiceRegistry *registry = service_registry_create();

	CHECK(embla != NULL && registry != NULL, "fixtures should allocate");

	Service *service = service_create("supervised", make_sleep_config());

	CHECK(service != NULL, "fixture should allocate");
	CHECK(
		service_registry_register(registry, service) == 0,
		"register service");

	CHECK(
		install_bounding_alarm(300000) == 0,
		"arming the bounding timer should succeed");

	int stopped = service_registry_supervise(registry, embla, 5.0);

	CHECK(
		alarm_fired == 1,
		"the bounding alarm should have fired during supervise()'s "
		"run, proving it was actually still looping, not returning "
		"immediately for some unrelated reason");
	CHECK(
		stopped >= 0,
		"supervise() should complete without a structural failure");
	CHECK(
		stopped == 1,
		"the one running service should have been stopped during "
		"the final orderly shutdown");
	CHECK(
		service_get_state(service) == SERVICE_STOPPED,
		"the service should have been started AND stopped by the "
		"time supervise() returns -- proving the full start -> "
		"run -> shutdown -> stop cycle actually happened, not just "
		"that supervise() returned some plausible-looking number");

	embla_clear_shutdown_request();

	service_registry_destroy(registry);
	embla_destroy(embla);

	return 0;
}

static int test_supervise_drives_restart_policy_for_multiple_services(void)
{
	embla_clear_shutdown_request();
	alarm_fired = 0;

	Embla *embla = embla_create();
	ServiceRegistry *registry = service_registry_create();

	CHECK(embla != NULL && registry != NULL, "fixtures should allocate");

	Service *crasher1 = service_create("crasher1", make_crash_config());
	Service *crasher2 = service_create("crasher2", make_crash_config());

	CHECK(
		crasher1 != NULL && crasher2 != NULL, "fixtures should allocate");

	service_set_restart_policy(crasher1, RESTART_ALWAYS);
	service_set_restart_base_delay(crasher1, 0.02);
	service_set_restart_policy(crasher2, RESTART_ALWAYS);
	service_set_restart_base_delay(crasher2, 0.02);

	CHECK(
		service_registry_register(registry, crasher1) == 0,
		"register crasher1");
	CHECK(
		service_registry_register(registry, crasher2) == 0,
		"register crasher2");

	CHECK(
		install_bounding_alarm(400000) == 0,
		"arming the bounding timer should succeed");

	service_registry_supervise(registry, embla, 5.0);

	CHECK(
		service_get_consecutive_restart_count(crasher1) > 1,
		"crasher1 should have been restarted more than once during "
		"the supervised window -- proving supervise()'s own loop "
		"drove restart policy, not just a single manual tick");
	CHECK(
		service_get_consecutive_restart_count(crasher2) > 1,
		"crasher2 should ALSO have been restarted more than once, "
		"simultaneously with crasher1 -- proving supervise() ticks "
		"EVERY registered service each cycle, not just the first "
		"one it finds");

	embla_clear_shutdown_request();

	service_registry_destroy(registry);
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
		{"shutdown_starts_unrequested", test_shutdown_starts_unrequested},
		{
			"request_shutdown_programmatic",
			test_request_shutdown_programmatic,
		},
		{"real_sigterm_sets_flag", test_real_sigterm_sets_flag},
		{
			"drain_handles_untracked_grandchild_gracefully",
			test_drain_handles_untracked_grandchild_gracefully,
		},
		{
			"supervise_runs_then_stops_on_request",
			test_supervise_runs_then_stops_on_request,
		},
		{
			"supervise_drives_restart_policy_for_multiple_services",
			test_supervise_drives_restart_policy_for_multiple_services,
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

	printf("all %zu root process / supervision tests passed\n", count);

	return EXIT_SUCCESS;
}
