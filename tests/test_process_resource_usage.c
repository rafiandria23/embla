
#include <stdio.h>
#include <stdlib.h>

#include "embla/process.h"

#define CHECK(cond, msg)                                                    \
	do                                                                      \
	{                                                                       \
		if (!(cond))                                                        \
		{                                                                   \
			fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
			return -1;                                                      \
		}                                                                   \
	} while (0)

static int test_defaults_are_unset_sentinels(void)
{
	Process *process = process_create(1, EMBLA_ROOT_PID, 1, "proc");

	CHECK(process != NULL, "create should succeed");
	CHECK(
		process_get_cpu_user_seconds(process) == -1.0,
		"cpu_user_seconds should default to the -1.0 unset "
		"sentinel, matching exit_code/term_signal's own -1 "
		"convention");
	CHECK(
		process_get_cpu_system_seconds(process) == -1.0,
		"cpu_system_seconds should default to -1.0");
	CHECK(
		process_get_max_rss_bytes(process) == -1,
		"max_rss_bytes should default to -1");

	process_destroy(process);

	return 0;
}

static int test_set_get_roundtrip(void)
{
	Process *process = process_create(1, EMBLA_ROOT_PID, 1, "proc");

	CHECK(process != NULL, "create should succeed");

	CHECK(
		process_set_cpu_user_seconds(process, 1.5) == 0,
		"setting cpu_user_seconds should succeed");
	CHECK(
		process_get_cpu_user_seconds(process) == 1.5,
		"cpu_user_seconds should read back exactly what was set");

	CHECK(
		process_set_cpu_system_seconds(process, 0.25) == 0,
		"setting cpu_system_seconds should succeed");
	CHECK(
		process_get_cpu_system_seconds(process) == 0.25,
		"cpu_system_seconds should read back exactly what was set");

	CHECK(
		process_set_max_rss_bytes(process, 4194304) == 0,
		"setting max_rss_bytes should succeed");
	CHECK(
		process_get_max_rss_bytes(process) == 4194304,
		"max_rss_bytes should read back exactly what was set");

	process_destroy(process);

	return 0;
}

static int test_null_safety(void)
{
	CHECK(
		process_get_cpu_user_seconds(NULL) == -1.0,
		"get_cpu_user_seconds(NULL) should be -1.0");
	CHECK(
		process_set_cpu_user_seconds(NULL, 1.0) != 0,
		"set_cpu_user_seconds(NULL, ...) should fail");
	CHECK(
		process_get_cpu_system_seconds(NULL) == -1.0,
		"get_cpu_system_seconds(NULL) should be -1.0");
	CHECK(
		process_set_cpu_system_seconds(NULL, 1.0) != 0,
		"set_cpu_system_seconds(NULL, ...) should fail");
	CHECK(
		process_get_max_rss_bytes(NULL) == -1,
		"get_max_rss_bytes(NULL) should be -1");
	CHECK(
		process_set_max_rss_bytes(NULL, 100) != 0,
		"set_max_rss_bytes(NULL, ...) should fail");

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
			"defaults_are_unset_sentinels",
			test_defaults_are_unset_sentinels,
		},
		{"set_get_roundtrip", test_set_get_roundtrip},
		{"null_safety", test_null_safety},
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

	printf("all %zu unit tests passed\n", count);

	return EXIT_SUCCESS;
}
