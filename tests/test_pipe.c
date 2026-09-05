#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "embla/pipe.h"

#define CHECK(cond, msg)                                                    \
	do                                                                      \
	{                                                                       \
		if (!(cond))                                                        \
		{                                                                   \
			fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
			return -1;                                                      \
		}                                                                   \
	} while (0)

static int test_create_gives_two_distinct_fds(void)
{
	Pipe *pipe = pipe_create();

	CHECK(pipe != NULL, "create should succeed");

	int read_fd = pipe_read_fd(pipe);
	int write_fd = pipe_write_fd(pipe);

	CHECK(read_fd >= 0 && write_fd >= 0, "both fds should be valid");
	CHECK(read_fd != write_fd, "read and write ends must be distinct fds");

	pipe_destroy(pipe);

	return 0;
}

static int test_created_ends_are_cloexec_by_default(void)
{
	Pipe *pipe = pipe_create();

	CHECK(pipe != NULL, "create should succeed");

	int read_flags = fcntl(pipe_read_fd(pipe), F_GETFD);
	int write_flags = fcntl(pipe_write_fd(pipe), F_GETFD);

	CHECK(
		(read_flags & FD_CLOEXEC) != 0,
		"the read end must be close-on-exec by default");
	CHECK(
		(write_flags & FD_CLOEXEC) != 0,
		"the write end must be close-on-exec by default");

	pipe_destroy(pipe);

	return 0;
}

static int test_clear_cloexec_actually_clears_it(void)
{
	Pipe *pipe = pipe_create();

	CHECK(pipe != NULL, "create should succeed");
	CHECK(
		pipe_clear_cloexec_read(pipe) == 0,
		"clearing close-on-exec on the read end should succeed");

	int read_flags = fcntl(pipe_read_fd(pipe), F_GETFD);

	CHECK(
		(read_flags & FD_CLOEXEC) == 0,
		"the read end's close-on-exec flag should now be cleared");

	int write_flags = fcntl(pipe_write_fd(pipe), F_GETFD);

	CHECK(
		(write_flags & FD_CLOEXEC) != 0,
		"clearing the read end must not affect the write end");

	pipe_destroy(pipe);

	return 0;
}

static int test_close_read_invalidates_only_read(void)
{
	Pipe *pipe = pipe_create();

	CHECK(pipe != NULL, "create should succeed");
	CHECK(pipe_close_read(pipe) == 0, "closing read should succeed");

	CHECK(
		pipe_read_fd(pipe) == -1,
		"the read accessor should return -1 once closed");
	CHECK(
		pipe_write_fd(pipe) != -1,
		"the write end must be unaffected by closing the read end");

	CHECK(
		pipe_clear_cloexec_read(pipe) != 0,
		"clearing close-on-exec on an already-closed end must fail");

	pipe_destroy(pipe);

	return 0;
}

static int test_double_close_is_idempotent(void)
{
	Pipe *pipe = pipe_create();

	CHECK(pipe != NULL, "create should succeed");
	CHECK(
		pipe_close_write(pipe) == 0,
		"first close should succeed");
	CHECK(
		pipe_close_write(pipe) == 0,
		"closing an already-closed end must be a safe no-op, not "
		"a failure");

	pipe_destroy(pipe);

	return 0;
}

static int test_destroy_null_is_safe(void)
{
	pipe_destroy(NULL);

	return 0;
}

static int test_null_safety(void)
{
	CHECK(pipe_read_fd(NULL) == -1, "read_fd(NULL) should be -1");
	CHECK(pipe_write_fd(NULL) == -1, "write_fd(NULL) should be -1");
	CHECK(
		pipe_clear_cloexec_read(NULL) != 0,
		"clear_cloexec_read(NULL) should fail");
	CHECK(
		pipe_clear_cloexec_write(NULL) != 0,
		"clear_cloexec_write(NULL) should fail");
	CHECK(
		pipe_close_read(NULL) != 0,
		"close_read(NULL) should fail");
	CHECK(
		pipe_close_write(NULL) != 0,
		"close_write(NULL) should fail");

	return 0;
}

int main(void)
{
	struct
	{
		const char *name;
		int (*fn)(void);
	} tests[] = {
		{"create_gives_two_distinct_fds", test_create_gives_two_distinct_fds},
		{
			"created_ends_are_cloexec_by_default",
			test_created_ends_are_cloexec_by_default,
		},
		{
			"clear_cloexec_actually_clears_it",
			test_clear_cloexec_actually_clears_it,
		},
		{
			"close_read_invalidates_only_read",
			test_close_read_invalidates_only_read,
		},
		{"double_close_is_idempotent", test_double_close_is_idempotent},
		{"destroy_null_is_safe", test_destroy_null_is_safe},
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
