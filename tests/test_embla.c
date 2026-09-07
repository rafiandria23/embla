#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "embla/embla.h"

#define CHECK(cond, msg)                                                    \
	do                                                                      \
	{                                                                       \
		if (!(cond))                                                        \
		{                                                                   \
			fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
			return -1;                                                      \
		}                                                                   \
	} while (0)

static int test_create_succeeds(void)
{
	Embla *embla = embla_create();

	CHECK(embla != NULL, "embla_create() should succeed");

	embla_destroy(embla);

	return 0;
}

static int test_create_returns_initialized_object(void)
{
	Embla *embla = embla_create();

	CHECK(embla != NULL, "embla_create() should succeed");
	CHECK(
		embla_process_manager(embla) != NULL,
		"a freshly created Embla should already have a process "
		"manager");
	CHECK(
		embla_process_group_manager(embla) != NULL,
		"a freshly created Embla should already have a process "
		"group manager");
	CHECK(
		embla_scheduler(embla) != NULL,
		"a freshly created Embla should already have a scheduler");
	CHECK(
		embla_executor(embla) != NULL,
		"a freshly created Embla should already have an executor");
	CHECK(
		embla_get_state(embla) == EMBLA_STOPPED,
		"a freshly created Embla should start in EMBLA_STOPPED, "
		"matching Phase 1's own stated invariant: \"new Embla -> "
		"stopped/inactive\"");

	embla_destroy(embla);

	return 0;
}

static int test_destroy_null_is_safe(void)
{
	embla_destroy(NULL);

	return 0;
}

static int test_destroy_freshly_created_object_is_safe(void)
{
	Embla *embla = embla_create();

	CHECK(embla != NULL, "embla_create() should succeed");

	embla_destroy(embla);

	return 0;
}

static int test_accessors_return_null_for_null_embla(void)
{
	CHECK(
		embla_process_manager(NULL) == NULL,
		"embla_process_manager(NULL) should return NULL");
	CHECK(
		embla_process_group_manager(NULL) == NULL,
		"embla_process_group_manager(NULL) should return NULL");
	CHECK(
		embla_scheduler(NULL) == NULL,
		"embla_scheduler(NULL) should return NULL");
	CHECK(
		embla_executor(NULL) == NULL,
		"embla_executor(NULL) should return NULL");

	return 0;
}

static int test_get_state_null_returns_stopped(void)
{
	CHECK(
		embla_get_state(NULL) == EMBLA_STOPPED,
		"embla_get_state(NULL) should default to EMBLA_STOPPED "
		"rather than crash or return an arbitrary value");

	return 0;
}

static int test_state_name_covers_every_defined_state(void)
{
	CHECK(
		strcmp(embla_state_name(EMBLA_STOPPED), "STOPPED") == 0,
		"EMBLA_STOPPED should map to \"STOPPED\"");
	CHECK(
		strcmp(embla_state_name(EMBLA_RUNNING), "RUNNING") == 0,
		"EMBLA_RUNNING should map to \"RUNNING\"");
	CHECK(
		strcmp(embla_state_name(EMBLA_STOPPING), "STOPPING") == 0,
		"EMBLA_STOPPING should map to \"STOPPING\"");

	return 0;
}

static int test_state_name_handles_unrecognized_value(void)
{
	CHECK(
		strcmp(embla_state_name((EmblaState)99), "UNKNOWN") == 0,
		"an out-of-range EmblaState value should fall back to "
		"\"UNKNOWN\" rather than crash or return garbage");

	return 0;
}

int main(void)
{
	struct
	{
		const char *name;
		int (*fn)(void);
	} tests[] = {
		{"create_succeeds", test_create_succeeds},
		{
			"create_returns_initialized_object",
			test_create_returns_initialized_object,
		},
		{"destroy_null_is_safe", test_destroy_null_is_safe},
		{
			"destroy_freshly_created_object_is_safe",
			test_destroy_freshly_created_object_is_safe,
		},
		{
			"accessors_return_null_for_null_embla",
			test_accessors_return_null_for_null_embla,
		},
		{
			"get_state_null_returns_stopped",
			test_get_state_null_returns_stopped,
		},
		{
			"state_name_covers_every_defined_state",
			test_state_name_covers_every_defined_state,
		},
		{
			"state_name_handles_unrecognized_value",
			test_state_name_handles_unrecognized_value,
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

	printf("all %zu Embla lifecycle tests passed\n", count);

	return EXIT_SUCCESS;
}
