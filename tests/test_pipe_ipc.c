#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "embla/executor.h"
#include "embla/pipe.h"
#include "embla/process.h"
#include "embla/process_config.h"

#define CHECK(cond, msg)                                                    \
	do                                                                      \
	{                                                                       \
		if (!(cond))                                                        \
		{                                                                   \
			fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
			return -1;                                                      \
		}                                                                   \
	} while (0)

static int test_pipe_roundtrip_via_cat(void)
{
	Pipe *stdin_pipe = pipe_create();
	Pipe *stdout_pipe = pipe_create();

	CHECK(
		stdin_pipe != NULL && stdout_pipe != NULL,
		"creating both pipes should succeed");

	CHECK(
		pipe_clear_cloexec_read(stdin_pipe) == 0,
		"clearing close-on-exec on the child's stdin end should "
		"succeed");
	CHECK(
		pipe_clear_cloexec_write(stdout_pipe) == 0,
		"clearing close-on-exec on the child's stdout end should "
		"succeed");

	char *argv[] = {"cat", NULL};
	ProcessConfig *config = process_config_create("/bin/cat", argv);

	CHECK(config != NULL, "process_config_create should succeed");
	CHECK(
		process_config_set_stdin_fd(
			config,
			pipe_read_fd(stdin_pipe)) == 0,
		"routing the child's stdin to the pipe should succeed");
	CHECK(
		process_config_set_stdout_fd(
			config,
			pipe_write_fd(stdout_pipe)) == 0,
		"routing the child's stdout to the pipe should succeed");

	Executor *executor = executor_create();
	Process *process = process_create(1, EMBLA_ROOT_PID, 1, "cat-proc");

	CHECK(executor != NULL && process != NULL, "fixtures should allocate");
	CHECK(
		executor_spawn(
			executor,
			process,
			EMBLA_INVALID_HOST_PGID,
			config) == 0,
		"spawning cat should succeed");
	CHECK(
		process_transition(process, PROCESS_READY) == 0,
		"transitioning to READY should succeed");

	CHECK(
		pipe_close_read(stdin_pipe) == 0,
		"parent should close its own copy of the child's stdin "
		"read end");
	CHECK(
		pipe_close_write(stdout_pipe) == 0,
		"parent MUST close its own copy of the child's stdout "
		"write end here -- otherwise the read below can never "
		"see EOF, even after cat exits (verified: omitting this "
		"causes a real, reproducible hang, not a theoretical one)");

	const char *message = "ping";
	ssize_t written = write(
		pipe_write_fd(stdin_pipe), message, strlen(message));

	CHECK(
		written == (ssize_t)strlen(message),
		"writing the test message to the child's stdin should "
		"succeed");

	CHECK(
		pipe_close_write(stdin_pipe) == 0,
		"closing the input write end should succeed");

	char output[64];
	size_t total = 0;

	for (;;)
	{
		ssize_t n = read(
			pipe_read_fd(stdout_pipe),
			output + total,
			sizeof(output) - total - 1);

		CHECK(n >= 0, "reading the child's output should not error");

		if (n == 0)
		{
			break;
		}

		total += (size_t)n;
	}

	output[total] = '\0';

	CHECK(
		strcmp(output, "ping") == 0,
		"the echoed content should match exactly what was written");

	CHECK(
		pipe_close_read(stdout_pipe) == 0,
		"closing the output read end should succeed");

	CHECK(
		executor_wait(executor, process, NULL) == 0,
		"waiting for the child to exit should succeed");
	CHECK(
		process_get_exit_code(process) == 0,
		"cat should have exited cleanly");

	process_destroy(process);
	executor_destroy(executor);
	process_config_destroy(config);
	pipe_destroy(stdin_pipe);
	pipe_destroy(stdout_pipe);

	return 0;
}

int main(void)
{
	printf("-- pipe_roundtrip_via_cat\n");

	if (test_pipe_roundtrip_via_cat() != 0)
	{
		fprintf(stderr, "pipe IPC round-trip test failed\n");
		return EXIT_FAILURE;
	}

	printf("pipe IPC round-trip test passed\n");

	return EXIT_SUCCESS;
}
