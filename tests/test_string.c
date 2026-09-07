#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "embla/string.h"

#define CHECK(cond, msg)                                                    \
	do                                                                      \
	{                                                                       \
		if (!(cond))                                                        \
		{                                                                   \
			fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
			return -1;                                                      \
		}                                                                   \
	} while (0)

static int test_strdup_null_returns_null(void)
{
	CHECK(
		embla_strdup(NULL) == NULL,
		"embla_strdup(NULL) should return NULL");

	return 0;
}

static int test_strdup_empty_string(void)
{
	char *result = embla_strdup("");

	CHECK(result != NULL, "duplicating an empty string should still allocate");
	CHECK(strcmp(result, "") == 0, "the copy should also be empty");

	free(result);

	return 0;
}

static int test_strdup_valid_string(void)
{
	char *result = embla_strdup("hello");

	CHECK(result != NULL, "duplicating a valid string should succeed");
	CHECK(
		strcmp(result, "hello") == 0,
		"the copy should read back exactly the original text");

	free(result);

	return 0;
}

static int test_strdup_is_a_deep_copy(void)
{
	char original[] = "mutable";

	char *copy = embla_strdup(original);

	CHECK(copy != NULL, "duplicating should succeed");
	CHECK(
		copy != original,
		"the copy must be a distinct allocation, not the same "
		"pointer that was passed in");

	original[0] = 'X';

	CHECK(
		strcmp(copy, "mutable") == 0,
		"mutating the ORIGINAL buffer after embla_strdup() returns "
		"must not affect the copy -- proves the copy is genuinely "
		"independent, not just equal at the moment it was made");

	free(copy);

	return 0;
}

int main(void)
{
	struct
	{
		const char *name;
		int (*fn)(void);
	} tests[] = {
		{"strdup_null_returns_null", test_strdup_null_returns_null},
		{"strdup_empty_string", test_strdup_empty_string},
		{"strdup_valid_string", test_strdup_valid_string},
		{"strdup_is_a_deep_copy", test_strdup_is_a_deep_copy},
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

	printf("all %zu string utility tests passed\n", count);

	return EXIT_SUCCESS;
}
