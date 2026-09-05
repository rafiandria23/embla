#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
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

static int run_server_child(const UnixListener *listener)
{
	int connection_fd = unix_listener_accept(listener);

	if (connection_fd < 0)
	{
		return 1;
	}

	char buf[64];
	ssize_t n = read(connection_fd, buf, sizeof(buf));

	if (n <= 0)
	{
		close(connection_fd);
		return 1;
	}

	if (write(connection_fd, buf, (size_t)n) != n)
	{
		close(connection_fd);
		return 1;
	}

	close(connection_fd);

	return 0;
}

static int run_roundtrip_test(void)
{
	char path[256];

	snprintf(path, sizeof(path), "/tmp/embla_test_ipc_%d.sock", getpid());
	unlink(path);

	UnixListener *listener = unix_listener_create(path, 1);

	CHECK(listener != NULL, "unix_listener_create should succeed");

	pid_t server_pid = fork();

	CHECK(server_pid >= 0, "fork should succeed");

	if (server_pid == 0)
	{
		_exit(run_server_child(listener));
	}

	int connection_fd = unix_socket_connect(path);

	CHECK(
		connection_fd >= 0,
		"connecting via the filesystem path should succeed");

	const char *message = "ping";
	ssize_t written = write(connection_fd, message, strlen(message));

	CHECK(
		written == (ssize_t)strlen(message),
		"writing the test message should succeed");

	char output[64];
	ssize_t n = read(connection_fd, output, sizeof(output) - 1);

	CHECK(n > 0, "reading the echoed response should succeed");

	output[n] = '\0';

	CHECK(
		strcmp(output, "ping") == 0,
		"the echoed content should match exactly, received back "
		"over a connection established purely via the filesystem "
		"path, not any fd inherited across fork()");

	close(connection_fd);

	int status;
	pid_t waited = waitpid(server_pid, &status, 0);

	CHECK(waited == server_pid, "waiting for the server child should succeed");
	CHECK(
		WIFEXITED(status) && WEXITSTATUS(status) == 0,
		"the server child should have exited cleanly");

	unix_listener_destroy(listener);

	return 0;
}

int main(void)
{
	printf("-- unix_socket_roundtrip_two_processes\n");

	if (run_roundtrip_test() != 0)
	{
		fprintf(stderr, "unix socket IPC round-trip test failed\n");
		return EXIT_FAILURE;
	}

	printf("unix socket IPC round-trip test passed\n");

	return EXIT_SUCCESS;
}
