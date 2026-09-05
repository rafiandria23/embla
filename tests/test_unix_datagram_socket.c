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

	snprintf(path, sizeof(path), "/tmp/embla_test_dgram_%d.sock", getpid());

	return path;
}

static int test_create_rejects_null_path(void)
{
	CHECK(
		unix_datagram_socket_create(NULL) == NULL,
		"a NULL path must be rejected");

	return 0;
}

static int test_create_rejects_oversized_path(void)
{
	char huge_path[512];

	memset(huge_path, 'x', sizeof(huge_path) - 1);
	huge_path[sizeof(huge_path) - 1] = '\0';

	CHECK(
		unix_datagram_socket_create(huge_path) == NULL,
		"a path too long to fit in sun_path must be rejected");

	return 0;
}

static int test_create_binds_a_real_filesystem_path(void)
{
	const char *path = test_socket_path();

	unlink(path);

	UnixDatagramSocket *socket = unix_datagram_socket_create(path);

	CHECK(socket != NULL, "create should succeed");

	struct stat st;

	CHECK(
		stat(path, &st) == 0,
		"the socket path should actually exist on disk after create");
	CHECK(
		S_ISSOCK(st.st_mode),
		"the created filesystem entry should be a socket");

	unix_datagram_socket_destroy(socket);

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

	UnixDatagramSocket *socket = unix_datagram_socket_create(path);

	CHECK(
		socket == NULL,
		"create must fail rather than silently removing an "
		"existing file at the path");

	unlink(path);

	return 0;
}

static int test_fd_accessor_and_cloexec_default(void)
{
	const char *path = test_socket_path();

	unlink(path);

	UnixDatagramSocket *socket = unix_datagram_socket_create(path);

	CHECK(socket != NULL, "create should succeed");

	int fd = unix_datagram_socket_fd(socket);

	CHECK(fd >= 0, "the fd accessor should return a valid fd");

	int flags = fcntl(fd, F_GETFD);

	CHECK(
		(flags & FD_CLOEXEC) != 0,
		"the bound fd must be close-on-exec by default");

	unix_datagram_socket_destroy(socket);

	return 0;
}

static int test_destroy_null_is_safe(void)
{
	unix_datagram_socket_destroy(NULL);

	return 0;
}

static int test_fd_accessor_null_is_safe(void)
{
	CHECK(
		unix_datagram_socket_fd(NULL) == -1,
		"fd accessor on NULL should return -1");

	return 0;
}

static int test_send_rejects_null_arguments(void)
{
	const char *path = test_socket_path();

	CHECK(
		unix_datagram_socket_send(NULL, "hello") == -1,
		"send with a NULL path must fail");
	CHECK(
		unix_datagram_socket_send(path, NULL) == -1,
		"send with a NULL message must fail");

	return 0;
}

static int test_send_with_nothing_listening_fails(void)
{
	const char *path = test_socket_path();

	unlink(path);

	CHECK(
		unix_datagram_socket_send(path, "hello") == -1,
		"sending to a path with nothing bound must fail, not hang "
		"or silently succeed");

	return 0;
}

static int test_send_and_receive_within_one_process(void)
{
	const char *path = test_socket_path();

	unlink(path);

	UnixDatagramSocket *socket = unix_datagram_socket_create(path);

	CHECK(socket != NULL, "create should succeed");
	CHECK(
		unix_datagram_socket_send(path, "READY=1\n") == 0,
		"sending should succeed");

	char buf[64];
	ssize_t n = read(unix_datagram_socket_fd(socket), buf, sizeof(buf) - 1);

	CHECK(n > 0, "reading the sent message should succeed");

	buf[n] = '\0';

	CHECK(
		strcmp(buf, "READY=1\n") == 0,
		"the received message should match exactly what was sent");

	unix_datagram_socket_destroy(socket);

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
		{
			"fd_accessor_and_cloexec_default",
			test_fd_accessor_and_cloexec_default,
		},
		{"destroy_null_is_safe", test_destroy_null_is_safe},
		{"fd_accessor_null_is_safe", test_fd_accessor_null_is_safe},
		{
			"send_rejects_null_arguments",
			test_send_rejects_null_arguments,
		},
		{
			"send_with_nothing_listening_fails",
			test_send_with_nothing_listening_fails,
		},
		{
			"send_and_receive_within_one_process",
			test_send_and_receive_within_one_process,
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
