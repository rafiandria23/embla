#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "embla/embla.h"
#include "embla/process_config.h"
#include "embla/service.h"
#include "embla/service_lifecycle.h"
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

static int find_index(Service **order, size_t count, Service *target)
{
	for (size_t i = 0; i < count; i++)
	{
		if (order[i] == target)
		{
			return (int)i;
		}
	}

	return -1;
}

static int test_linear_chain_order(void)
{
	ServiceRegistry *registry = service_registry_create();

	CHECK(registry != NULL, "registry create should succeed");

	Service *a = service_create("a", make_sleep_config());
	Service *b = service_create("b", make_sleep_config());
	Service *c = service_create("c", make_sleep_config());

	CHECK(a != NULL && b != NULL && c != NULL, "fixtures should allocate");
	CHECK(service_add_dependency(a, "b") == 0, "a depends on b");
	CHECK(service_add_dependency(b, "c") == 0, "b depends on c");

	CHECK(service_registry_register(registry, a) == 0, "register a");
	CHECK(service_registry_register(registry, b) == 0, "register b");
	CHECK(service_registry_register(registry, c) == 0, "register c");

	Service *order[3];
	size_t order_count;

	CHECK(
		service_registry_compute_start_order(registry, order, &order_count) == 0,
		"computing the order should succeed for an acyclic graph");
	CHECK(order_count == 3, "the order should include all 3 services");

	int index_a = find_index(order, order_count, a);
	int index_b = find_index(order, order_count, b);
	int index_c = find_index(order, order_count, c);

	CHECK(
		index_c < index_b && index_b < index_a,
		"the order must be C before B before A -- exactly reversed "
		"from the dependency declarations (A depends on B depends "
		"on C)");

	service_registry_destroy(registry);

	return 0;
}

static int test_diamond_dependency_order(void)
{
	ServiceRegistry *registry = service_registry_create();

	CHECK(registry != NULL, "registry create should succeed");

	Service *a = service_create("a", make_sleep_config());
	Service *b = service_create("b", make_sleep_config());
	Service *c = service_create("c", make_sleep_config());
	Service *d = service_create("d", make_sleep_config());

	CHECK(
		a != NULL && b != NULL && c != NULL && d != NULL,
		"fixtures should allocate");
	CHECK(service_add_dependency(a, "b") == 0, "a depends on b");
	CHECK(service_add_dependency(a, "c") == 0, "a depends on c");
	CHECK(service_add_dependency(b, "d") == 0, "b depends on d");
	CHECK(service_add_dependency(c, "d") == 0, "c depends on d");

	CHECK(service_registry_register(registry, a) == 0, "register a");
	CHECK(service_registry_register(registry, b) == 0, "register b");
	CHECK(service_registry_register(registry, c) == 0, "register c");
	CHECK(service_registry_register(registry, d) == 0, "register d");

	Service *order[4];
	size_t order_count;

	CHECK(
		service_registry_compute_start_order(registry, order, &order_count) == 0,
		"computing the order should succeed");
	CHECK(order_count == 4, "the order should include all 4 services");

	int index_a = find_index(order, order_count, a);
	int index_b = find_index(order, order_count, b);
	int index_c = find_index(order, order_count, c);
	int index_d = find_index(order, order_count, d);

	CHECK(
		index_d < index_b && index_d < index_c,
		"d must come before both b and c");
	CHECK(
		index_a > index_b && index_a > index_c,
		"a must come after both b and c");

	service_registry_destroy(registry);

	return 0;
}

static int test_cycle_is_detected(void)
{
	ServiceRegistry *registry = service_registry_create();

	CHECK(registry != NULL, "registry create should succeed");

	Service *a = service_create("a", make_sleep_config());
	Service *b = service_create("b", make_sleep_config());

	CHECK(a != NULL && b != NULL, "fixtures should allocate");
	CHECK(service_add_dependency(a, "b") == 0, "a depends on b");
	CHECK(service_add_dependency(b, "a") == 0, "b depends on a -- a cycle");

	CHECK(service_registry_register(registry, a) == 0, "register a");
	CHECK(service_registry_register(registry, b) == 0, "register b");

	Service *order[2];
	size_t order_count;

	CHECK(
		service_registry_compute_start_order(registry, order, &order_count) != 0,
		"a direct A<->B cycle must be detected and rejected");

	service_registry_destroy(registry);

	return 0;
}

static int test_missing_dependency_is_detected(void)
{
	ServiceRegistry *registry = service_registry_create();

	CHECK(registry != NULL, "registry create should succeed");

	Service *a = service_create("a", make_sleep_config());

	CHECK(a != NULL, "fixture should allocate");
	CHECK(
		service_add_dependency(a, "nonexistent") == 0,
		"declaring the dependency itself should succeed -- it's "
		"only checked for existence when the order is computed");

	CHECK(service_registry_register(registry, a) == 0, "register a");

	Service *order[1];
	size_t order_count;

	CHECK(
		service_registry_compute_start_order(registry, order, &order_count) != 0,
		"a dependency on an unregistered name must be detected and "
		"rejected");

	service_registry_destroy(registry);

	return 0;
}

static int test_self_dependency_rejected_at_declaration(void)
{
	Service *a = service_create("a", make_sleep_config());

	CHECK(a != NULL, "fixture should allocate");
	CHECK(
		service_add_dependency(a, "a") != 0,
		"a service depending on its own name must be rejected "
		"immediately");
	CHECK(
		service_get_dependency_count(a) == 0,
		"the rejected dependency must not have been added");

	service_destroy(a);

	return 0;
}

static int test_start_all_respects_real_order(void)
{
	Embla *embla = embla_create();
	ServiceRegistry *registry = service_registry_create();

	CHECK(embla != NULL && registry != NULL, "fixtures should allocate");

	Service *a = service_create("a", make_sleep_config());
	Service *b = service_create("b", make_sleep_config());

	CHECK(a != NULL && b != NULL, "fixtures should allocate");
	CHECK(service_add_dependency(a, "b") == 0, "a depends on b");

	CHECK(service_registry_register(registry, a) == 0, "register a");
	CHECK(service_registry_register(registry, b) == 0, "register b");

	int started = service_registry_start_all(registry, embla);

	CHECK(started == 2, "both services should start successfully");
	CHECK(
		service_get_state(a) == SERVICE_RUNNING &&
			service_get_state(b) == SERVICE_RUNNING,
		"both should now be running");
	CHECK(
		service_get_last_start_time(b) <= service_get_last_start_time(a),
		"b's recorded start time must be at or before a's -- proving "
		"the dependency was actually started first, not just "
		"computed as first in the order");

	service_registry_stop_all(registry, embla, 5.0);
	service_registry_destroy(registry);
	embla_destroy(embla);

	return 0;
}

static int test_start_all_skips_dependent_of_spawn_failure(void)
{
	Embla *embla = embla_create();
	ServiceRegistry *registry = service_registry_create();

	CHECK(embla != NULL && registry != NULL, "fixtures should allocate");

	Process *fillers[16];
	ProcessConfig *filler_configs[16];

	for (int i = 0; i < 16; i++)
	{
		char name[16];

		snprintf(name, sizeof(name), "filler%d", i);

		filler_configs[i] = make_sleep_config();

		CHECK(filler_configs[i] != NULL, "filler config should allocate");

		fillers[i] = embla_spawn(embla, name, filler_configs[i]);

		CHECK(
			fillers[i] != NULL,
			"filler spawn should succeed while capacity remains");
	}

	Service *broken = service_create("broken", make_sleep_config());
	Service *dependent = service_create("dependent", make_sleep_config());

	CHECK(broken != NULL && dependent != NULL, "fixtures should allocate");
	CHECK(
		service_add_dependency(dependent, "broken") == 0,
		"dependent depends on broken");

	CHECK(
		service_registry_register(registry, broken) == 0,
		"register broken");
	CHECK(
		service_registry_register(registry, dependent) == 0,
		"register dependent");

	int started = service_registry_start_all(registry, embla);

	CHECK(
		started == 0,
		"broken's own spawn should genuinely fail -- "
		"ProcessGroupManager's capacity is already exhausted by "
		"the 16 filler processes");
	CHECK(
		service_get_state(broken) == SERVICE_STOPPED,
		"broken should remain SERVICE_STOPPED after its failed "
		"spawn attempt");
	CHECK(
		service_get_state(dependent) == SERVICE_STOPPED,
		"dependent must be skipped -- its dependency never became "
		"running");
	CHECK(
		service_get_process(dependent) == NULL,
		"dependent should have no process at all -- it was never "
		"even attempted, not attempted-and-failed");

	for (int i = 0; i < 16; i++)
	{
		embla_kill(embla, fillers[i]);
		process_config_destroy(filler_configs[i]);
	}

	embla_run(embla);

	service_registry_destroy(registry);
	embla_destroy(embla);

	return 0;
}

static int test_stop_all_respects_reverse_order(void)
{
	Embla *embla = embla_create();
	ServiceRegistry *registry = service_registry_create();

	CHECK(embla != NULL && registry != NULL, "fixtures should allocate");

	Service *a = service_create("a", make_sleep_config());
	Service *b = service_create("b", make_sleep_config());

	CHECK(a != NULL && b != NULL, "fixtures should allocate");
	CHECK(service_add_dependency(a, "b") == 0, "a depends on b");

	CHECK(service_registry_register(registry, a) == 0, "register a");
	CHECK(service_registry_register(registry, b) == 0, "register b");

	CHECK(
		service_registry_start_all(registry, embla) == 2,
		"both should start");

	int stopped = service_registry_stop_all(registry, embla, 5.0);

	CHECK(stopped == 2, "both services should stop successfully");
	CHECK(
		service_get_state(a) == SERVICE_STOPPED &&
			service_get_state(b) == SERVICE_STOPPED,
		"both should now be stopped");

	service_registry_destroy(registry);
	embla_destroy(embla);

	return 0;
}

static int test_stop_all_escalates_to_sigkill(void)
{
	Embla *embla = embla_create();
	ServiceRegistry *registry = service_registry_create();

	CHECK(embla != NULL && registry != NULL, "fixtures should allocate");

	char *argv[] = {"sh", "-c", "trap '' TERM; sleep 30", NULL};
	ProcessConfig *config = process_config_create("/bin/sh", argv);

	CHECK(config != NULL, "config create should succeed");

	Service *stubborn = service_create("stubborn", config);

	CHECK(stubborn != NULL, "fixture should allocate");
	CHECK(
		service_registry_register(registry, stubborn) == 0,
		"register stubborn");

	CHECK(
		service_registry_start_all(registry, embla) == 1,
		"stubborn should start");

	struct timespec settle = {.tv_sec = 0, .tv_nsec = 100000000};

	nanosleep(&settle, NULL);

	int stopped = service_registry_stop_all(registry, embla, 0.3);

	CHECK(
		stopped == 1,
		"stubborn should still end up stopped, via SIGKILL "
		"escalation, despite ignoring SIGTERM");
	CHECK(
		service_get_state(stubborn) == SERVICE_STOPPED,
		"stubborn should now read as SERVICE_STOPPED");
	CHECK(
		service_get_last_term_signal(stubborn) == SIGKILL,
		"the recorded terminating signal should be SIGKILL "
		"specifically, proving escalation genuinely happened -- "
		"not that the process happened to exit on its own for "
		"some unrelated reason");

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
		{"linear_chain_order", test_linear_chain_order},
		{"diamond_dependency_order", test_diamond_dependency_order},
		{"cycle_is_detected", test_cycle_is_detected},
		{
			"missing_dependency_is_detected",
			test_missing_dependency_is_detected,
		},
		{
			"self_dependency_rejected_at_declaration",
			test_self_dependency_rejected_at_declaration,
		},
		{
			"start_all_respects_real_order",
			test_start_all_respects_real_order,
		},
		{
			"start_all_skips_dependent_of_spawn_failure",
			test_start_all_skips_dependent_of_spawn_failure,
		},
		{
			"stop_all_respects_reverse_order",
			test_stop_all_respects_reverse_order,
		},
		{
			"stop_all_escalates_to_sigkill",
			test_stop_all_escalates_to_sigkill,
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

	printf("all %zu orchestrator tests passed\n", count);

	return EXIT_SUCCESS;
}
