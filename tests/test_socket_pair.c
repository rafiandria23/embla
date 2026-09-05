#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "embla/socket_pair.h"

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
	SocketPair *pair = socket_pair_create(SOCK_STREAM);

	CHECK(pair != NULL, "create should succeed");

	int first_fd = socket_pair_first_fd(pair);
	int second_fd = socket_pair_second_fd(pair);

	CHECK(first_fd >= 0 && second_fd >= 0, "both fds should be valid");
	CHECK(first_fd != second_fd, "the two ends must be distinct fds");

	socket_pair_destroy(pair);

	return 0;
}

static int test_created_ends_are_cloexec_by_default(void)
{
	SocketPair *pair = socket_pair_create(SOCK_STREAM);

	CHECK(pair != NULL, "create should succeed");

	int first_flags = fcntl(socket_pair_first_fd(pair), F_GETFD);
	int second_flags = fcntl(socket_pair_second_fd(pair), F_GETFD);

	CHECK(
		(first_flags & FD_CLOEXEC) != 0,
		"the first end must be close-on-exec by default");
	CHECK(
		(second_flags & FD_CLOEXEC) != 0,
		"the second end must be close-on-exec by default");

	socket_pair_destroy(pair);

	return 0;
}

static int test_clear_cloexec_actually_clears_it(void)
{
	SocketPair *pair = socket_pair_create(SOCK_STREAM);

	CHECK(pair != NULL, "create should succeed");
	CHECK(
		socket_pair_clear_cloexec_first(pair) == 0,
		"clearing close-on-exec on the first end should succeed");

	int first_flags = fcntl(socket_pair_first_fd(pair), F_GETFD);

	CHECK(
		(first_flags & FD_CLOEXEC) == 0,
		"the first end's close-on-exec flag should now be cleared");

	int second_flags = fcntl(socket_pair_second_fd(pair), F_GETFD);

	CHECK(
		(second_flags & FD_CLOEXEC) != 0,
		"clearing the first end must not affect the second end");

	socket_pair_destroy(pair);

	return 0;
}

static int test_close_first_invalidates_only_first(void)
{
	SocketPair *pair = socket_pair_create(SOCK_STREAM);

	CHECK(pair != NULL, "create should succeed");
	CHECK(socket_pair_close_first(pair) == 0, "closing first should succeed");

	CHECK(
		socket_pair_first_fd(pair) == -1,
		"the first accessor should return -1 once closed");
	CHECK(
		socket_pair_second_fd(pair) != -1,
		"the second end must be unaffected by closing the first end");

	CHECK(
		socket_pair_clear_cloexec_first(pair) != 0,
		"clearing close-on-exec on an already-closed end must fail");

	socket_pair_destroy(pair);

	return 0;
}

static int test_double_close_is_idempotent(void)
{
	SocketPair *pair = socket_pair_create(SOCK_STREAM);

	CHECK(pair != NULL, "create should succeed");
	CHECK(
		socket_pair_close_second(pair) == 0,
		"first close should succeed");
	CHECK(
		socket_pair_close_second(pair) == 0,
		"closing an already-closed end must be a safe no-op, not "
		"a failure");

	socket_pair_destroy(pair);

	return 0;
}

static int test_destroy_null_is_safe(void)
{
	socket_pair_destroy(NULL);

	return 0;
}

static int test_null_safety(void)
{
	CHECK(socket_pair_first_fd(NULL) == -1, "first_fd(NULL) should be -1");
	CHECK(socket_pair_second_fd(NULL) == -1, "second_fd(NULL) should be -1");
	CHECK(
		socket_pair_clear_cloexec_first(NULL) != 0,
		"clear_cloexec_first(NULL) should fail");
	CHECK(
		socket_pair_clear_cloexec_second(NULL) != 0,
		"clear_cloexec_second(NULL) should fail");
	CHECK(
		socket_pair_close_first(NULL) != 0,
		"close_first(NULL) should fail");
	CHECK(
		socket_pair_close_second(NULL) != 0,
		"close_second(NULL) should fail");

	return 0;
}

static int test_bidirectional_in_both_directions(void)
{
	SocketPair *pair = socket_pair_create(SOCK_STREAM);

	CHECK(pair != NULL, "create should succeed");

	int first_fd = socket_pair_first_fd(pair);
	int second_fd = socket_pair_second_fd(pair);

	CHECK(
		write(first_fd, "abc", 3) == 3,
		"writing on the first end should succeed");

	char buf1[8] = {0};

	CHECK(
		read(second_fd, buf1, sizeof(buf1)) == 3,
		"reading on the second end should receive what the first "
		"end sent");
	CHECK(
		memcmp(buf1, "abc", 3) == 0,
		"content should match, first -> second");

	CHECK(
		write(second_fd, "xyz", 3) == 3,
		"writing on the second end should succeed -- the SAME "
		"pair, opposite direction");

	char buf2[8] = {0};

	CHECK(
		read(first_fd, buf2, sizeof(buf2)) == 3,
		"reading on the first end should receive what the second "
		"end sent");
	CHECK(
		memcmp(buf2, "xyz", 3) == 0,
		"content should match, second -> first -- proving genuine "
		"bidirectionality on one pair, not just two one-way fds");

	socket_pair_destroy(pair);

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
			"close_first_invalidates_only_first",
			test_close_first_invalidates_only_first,
		},
		{"double_close_is_idempotent", test_double_close_is_idempotent},
		{"destroy_null_is_safe", test_destroy_null_is_safe},
		{"null_safety", test_null_safety},
		{
			"bidirectional_in_both_directions",
			test_bidirectional_in_both_directions,
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

	printf("all %zu unit tests passed\n", count);

	return EXIT_SUCCESS;
}
