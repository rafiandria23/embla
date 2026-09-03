
#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "embla/unix_socket.h"

#define CHECK(cond, msg)                                                    \
	do                                                                      \
	{                                                                       \
		if (!(cond))                                                        \
		{                                                                   \
			fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
			return -1;                                                      \
		}                                                                   \
	} while (0)

static const char *test_socket_path(void)
{
	static char path[256];

	snprintf(path, sizeof(path), "/tmp/embla_test_unit_%d.sock", getpid());

	return path;
}

static int test_create_rejects_null_path(void)
{
	CHECK(
		unix_listener_create(NULL, 1) == NULL,
		"a NULL path must be rejected");
	CHECK(
		unix_socket_connect(NULL) == -1,
		"connecting to a NULL path must be rejected");

	return 0;
}

static int test_create_rejects_oversized_path(void)
{
	char huge_path[512];

	memset(huge_path, 'x', sizeof(huge_path) - 1);
	huge_path[sizeof(huge_path) - 1] = '\0';

	CHECK(
		unix_listener_create(huge_path, 1) == NULL,
		"a path too long to fit in sun_path must be rejected");

	return 0;
}

static int test_create_binds_a_real_filesystem_path(void)
{
	const char *path = test_socket_path();

	unlink(path);

	UnixListener *listener = unix_listener_create(path, 1);

	CHECK(listener != NULL, "create should succeed");

	struct stat st;

	CHECK(
		stat(path, &st) == 0,
		"the socket path should actually exist on disk after create");
	CHECK(
		S_ISSOCK(st.st_mode),
		"the created filesystem entry should be a socket");

	unix_listener_destroy(listener);

	CHECK(
		stat(path, &st) != 0,
		"the socket path should be gone after destroy (unlinked)");

	return 0;
}

static int test_create_does_not_auto_unlink_existing_path(void)
{
	const char *path = test_socket_path();

	unlink(path);

	FILE *f = fopen(path, "w");

	CHECK(f != NULL, "creating a placeholder file should succeed");
	fclose(f);

	UnixListener *listener = unix_listener_create(path, 1);

	CHECK(
		listener == NULL,
		"create must fail rather than silently removing an "
		"existing file at the path -- this module never unlinks "
		"anything it didn't itself create");

	unlink(path);

	return 0;
}

static int test_listener_fd_accessor(void)
{
	const char *path = test_socket_path();

	unlink(path);

	UnixListener *listener = unix_listener_create(path, 1);

	CHECK(listener != NULL, "create should succeed");
	CHECK(
		unix_listener_fd(listener) >= 0,
		"the listening fd accessor should return a valid fd");

	unix_listener_destroy(listener);

	return 0;
}

static int test_listener_fd_is_cloexec_by_default(void)
{
	const char *path = test_socket_path();

	unlink(path);

	UnixListener *listener = unix_listener_create(path, 1);

	CHECK(listener != NULL, "create should succeed");

	int flags = fcntl(unix_listener_fd(listener), F_GETFD);

	CHECK(
		(flags & FD_CLOEXEC) != 0,
		"the listening fd must be close-on-exec by default");

	unix_listener_destroy(listener);

	return 0;
}

static int test_destroy_null_is_safe(void)
{
	unix_listener_destroy(NULL);

	return 0;
}

static int test_accept_null_listener_fails(void)
{
	CHECK(
		unix_listener_accept(NULL) == -1,
		"accepting on a NULL listener must fail");

	return 0;
}

static int test_connect_with_nothing_listening_fails(void)
{
	const char *path = test_socket_path();

	unlink(path);

	CHECK(
		unix_socket_connect(path) == -1,
		"connecting to a path with no listener must fail, not hang");

	return 0;
}

int main(void)
{
	struct
	{
		const char *name;
		int (*fn)(void);
	} tests[] = {
		{"create_rejects_null_path", test_create_rejects_null_path},
		{
			"create_rejects_oversized_path",
			test_create_rejects_oversized_path,
		},
		{
			"create_binds_a_real_filesystem_path",
			test_create_binds_a_real_filesystem_path,
		},
		{
			"create_does_not_auto_unlink_existing_path",
			test_create_does_not_auto_unlink_existing_path,
		},
		{"listener_fd_accessor", test_listener_fd_accessor},
		{
			"listener_fd_is_cloexec_by_default",
			test_listener_fd_is_cloexec_by_default,
		},
		{"destroy_null_is_safe", test_destroy_null_is_safe},
		{"accept_null_listener_fails", test_accept_null_listener_fails},
		{
			"connect_with_nothing_listening_fails",
			test_connect_with_nothing_listening_fails,
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
