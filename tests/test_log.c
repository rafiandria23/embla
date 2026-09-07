#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "embla/log.h"

#define CHECK(cond, msg)                                                    \
	do                                                                      \
	{                                                                       \
		if (!(cond))                                                        \
		{                                                                   \
			fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
			return -1;                                                      \
		}                                                                   \
	} while (0)

static char *capture_output(
	FILE *stream,
	int fd,
	void (*log_fn)(const char *),
	const char *message)
{
	int saved_fd = dup(fd);

	if (saved_fd < 0)
	{
		return NULL;
	}

	char path[] = "/tmp/embla_log_test_XXXXXX";
	int temp_fd = mkstemp(path);

	if (temp_fd < 0)
	{
		close(saved_fd);
		return NULL;
	}

	fflush(stream);

	if (dup2(temp_fd, fd) < 0)
	{
		close(temp_fd);
		close(saved_fd);
		unlink(path);
		return NULL;
	}

	close(temp_fd);

	log_fn(message);

	fflush(stream);
	dup2(saved_fd, fd);
	close(saved_fd);

	FILE *readback = fopen(path, "r");

	unlink(path);

	if (readback == NULL)
	{
		return NULL;
	}

	char buffer[512] = {0};

	size_t read_count = fread(buffer, 1, sizeof(buffer) - 1, readback);

	fclose(readback);

	(void)read_count;

	char *result = malloc(strlen(buffer) + 1);

	if (result != NULL)
	{
		strcpy(result, buffer);
	}

	return result;
}

static int test_info_and_error_can_be_called_with_a_message(void)
{
	embla_log_info("a normal informational message");
	embla_log_error("a normal error message");

	return 0;
}

static int test_info_and_error_tolerate_null(void)
{
	char *info_output = capture_output(
		stdout,
		STDOUT_FILENO,
		embla_log_info,
		NULL);

	CHECK(info_output != NULL, "capturing stdout should succeed");
	CHECK(
		strstr(info_output, "(null)") != NULL,
		"a NULL message to embla_log_info should be substituted "
		"with the literal text \"(null)\", not crash or print "
		"nothing");

	free(info_output);

	char *error_output = capture_output(stderr, STDERR_FILENO, embla_log_error, NULL);

	CHECK(error_output != NULL, "capturing stderr should succeed");
	CHECK(
		strstr(error_output, "(null)") != NULL,
		"the same NULL-tolerance should hold for embla_log_error");

	free(error_output);

	return 0;
}

static int test_info_and_error_tolerate_empty_string(void)
{
	embla_log_info("");
	embla_log_error("");

	return 0;
}

static int test_info_has_stable_prefix(void)
{
	char *output = capture_output(
		stdout,
		STDOUT_FILENO,
		embla_log_info,
		"checking prefix");

	CHECK(output != NULL, "capturing stdout should succeed");
	CHECK(
		strncmp(output, "[INFO]", 6) == 0,
		"embla_log_info's output should begin with the stable "
		"\"[INFO]\" prefix");
	CHECK(
		strstr(output, "checking prefix") != NULL,
		"the actual message content should also appear in the "
		"output, not just the prefix");

	free(output);

	return 0;
}

static int test_error_has_stable_prefix(void)
{
	char *output = capture_output(
		stderr,
		STDERR_FILENO,
		embla_log_error,
		"checking prefix");

	CHECK(output != NULL, "capturing stderr should succeed");
	CHECK(
		strncmp(output, "[ERROR]", 7) == 0,
		"embla_log_error's output should begin with the stable "
		"\"[ERROR]\" prefix");
	CHECK(
		strstr(output, "checking prefix") != NULL,
		"the actual message content should also appear in the "
		"output, not just the prefix");

	free(output);

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
			"info_and_error_can_be_called_with_a_message",
			test_info_and_error_can_be_called_with_a_message,
		},
		{
			"info_and_error_tolerate_null",
			test_info_and_error_tolerate_null,
		},
		{
			"info_and_error_tolerate_empty_string",
			test_info_and_error_tolerate_empty_string,
		},
		{"info_has_stable_prefix", test_info_has_stable_prefix},
		{"error_has_stable_prefix", test_error_has_stable_prefix},
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

	printf("all %zu logging tests passed\n", count);

	return EXIT_SUCCESS;
}
