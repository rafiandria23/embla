#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "embla/executor.h"
#include "embla/process.h"
#include "embla/process_config.h"
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

static int test_socket_pair_roundtrip_via_cat(void)
{
	SocketPair *pair = socket_pair_create(SOCK_STREAM);

	CHECK(pair != NULL, "socket_pair_create should succeed");
	CHECK(
		socket_pair_clear_cloexec_second(pair) == 0,
		"clearing close-on-exec on the child's end should succeed");

	int child_fd = socket_pair_second_fd(pair);

	char *argv[] = {"cat", NULL};
	ProcessConfig *config = process_config_create("/bin/cat", argv);

	CHECK(config != NULL, "process_config_create should succeed");
	CHECK(
		process_config_set_stdin_fd(config, child_fd) == 0,
		"routing the child's stdin to the socket should succeed");
	CHECK(
		process_config_set_stdout_fd(config, child_fd) == 0,
		"routing the child's stdout to the SAME socket fd should "
		"succeed -- legitimate specifically because a socketpair "
		"end genuinely supports both directions, unlike a pipe end");

	Executor *executor = executor_create();
	Process *process = process_create(1, EMBLA_ROOT_PID, 1, "cat-proc");

	CHECK(executor != NULL && process != NULL, "fixtures should allocate");
	CHECK(
		executor_spawn(
			executor, process, EMBLA_INVALID_HOST_PGID, config) == 0,
		"spawning cat should succeed");
	CHECK(
		process_transition(process, PROCESS_READY) == 0,
		"transitioning to READY should succeed");

	CHECK(
		socket_pair_close_second(pair) == 0,
		"parent should close its own copy of the child's end");

	int parent_fd = socket_pair_first_fd(pair);

	const char *message = "ping";
	ssize_t written = write(parent_fd, message, strlen(message));

	CHECK(
		written == (ssize_t)strlen(message),
		"writing the test message should succeed");

	CHECK(
		shutdown(parent_fd, SHUT_WR) == 0,
		"shutdown(SHUT_WR) should succeed");

	char output[64];
	size_t total = 0;

	for (;;)
	{
		ssize_t n = read(
			parent_fd,
			output + total,
			sizeof(output) - total - 1);

		CHECK(n >= 0, "reading the echoed response should not error");

		if (n == 0)
		{
			break;
		}

		total += (size_t)n;
	}

	output[total] = '\0';

	CHECK(
		strcmp(output, "ping") == 0,
		"the echoed content should match exactly, received back "
		"over the SAME fd that sent it");

	CHECK(
		executor_wait(executor, process, NULL) == 0,
		"waiting for the child to exit should succeed");
	CHECK(
		process_get_exit_code(process) == 0,
		"cat should have exited cleanly");

	process_destroy(process);
	executor_destroy(executor);
	process_config_destroy(config);
	socket_pair_destroy(pair);

	return 0;
}

int main(void)
{
	printf("-- socket_pair_roundtrip_via_cat\n");

	if (test_socket_pair_roundtrip_via_cat() != 0)
	{
		fprintf(stderr, "socket pair IPC round-trip test failed\n");
		return EXIT_FAILURE;
	}

	printf("socket pair IPC round-trip test passed\n");

	return EXIT_SUCCESS;
}
