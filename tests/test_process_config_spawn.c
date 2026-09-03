
#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "embla/executor.h"
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

static void set_cloexec(int fd)
{
	fcntl(fd, F_SETFD, FD_CLOEXEC);
}

static int read_all_trimmed(
	int fd,
	char *buf,
	size_t buf_size)
{
	size_t total = 0;

	for (;;)
	{
		if (total + 1 >= buf_size)
		{
			return -1;
		}

		ssize_t n = read(fd, buf + total, buf_size - total - 1);

		if (n < 0)
		{
			return -1;
		}

		if (n == 0)
		{
			break;
		}

		total += (size_t)n;
	}

	buf[total] = '\0';

	if (total > 0 && buf[total - 1] == '\n')
	{
		buf[total - 1] = '\0';
	}

	return 0;
}

static int spawn_and_wait(
	const ProcessConfig *config,
	int *out_exit_code)
{
	Executor *executor = executor_create();

	if (executor == NULL)
	{
		return -1;
	}

	Process *process = process_create(1, EMBLA_ROOT_PID, 1, "test-process");

	if (process == NULL)
	{
		executor_destroy(executor);
		return -1;
	}

	int result = 0;

	if (
		executor_spawn(
			executor, process, EMBLA_INVALID_HOST_PGID, config) != 0)
	{
		result = -1;
	}
	else if (process_transition(process, PROCESS_READY) != 0)
	{
		result = -1;
	}
	else if (executor_wait(executor, process, NULL) != 0)
	{
		result = -1;
	}
	else
	{
		*out_exit_code = process_get_exit_code(process);
	}

	process_destroy(process);
	executor_destroy(executor);

	return result;
}

static int test_working_directory_applied(void)
{
	int pipe_out[2];

	CHECK(pipe(pipe_out) == 0, "creating the output pipe should succeed");
	set_cloexec(pipe_out[0]);

	char *argv[] = {"sh", "-c", "pwd", NULL};
	ProcessConfig *config = process_config_create("/bin/sh", argv);

	CHECK(config != NULL, "process_config_create should succeed");
	CHECK(
		process_config_set_working_directory(config, "/tmp") == 0,
		"setting working directory should succeed");
	CHECK(
		process_config_set_stdout_fd(config, pipe_out[1]) == 0,
		"setting stdout fd should succeed");

	int exit_code;
	int spawn_result = spawn_and_wait(config, &exit_code);

	close(pipe_out[1]);

	CHECK(spawn_result == 0, "spawning with a working directory should succeed");
	CHECK(exit_code == 0, "the spawned shell should exit cleanly");

	char output[256];

	CHECK(
		read_all_trimmed(pipe_out[0], output, sizeof(output)) == 0,
		"reading the captured pwd output should succeed");

	char resolved_tmp[PATH_MAX];

	CHECK(
		realpath("/tmp", resolved_tmp) != NULL,
		"resolving /tmp in this test process should succeed");
	CHECK(
		strcmp(output, resolved_tmp) == 0,
		"the child's working directory should match wherever /tmp "
		"actually resolves to on this system");

	close(pipe_out[0]);
	process_config_destroy(config);

	return 0;
}

static int test_env_explicit(void)
{
	int pipe_out[2];

	CHECK(pipe(pipe_out) == 0, "creating the output pipe should succeed");
	set_cloexec(pipe_out[0]);

	char *argv[] = {"sh", "-c", "printf '%s' \"$EMBLA_TEST_VAR\"", NULL};
	ProcessConfig *config = process_config_create("/bin/sh", argv);

	CHECK(config != NULL, "process_config_create should succeed");

	char *envp[] = {"EMBLA_TEST_VAR=hello123", NULL};

	CHECK(
		process_config_set_env(config, envp) == 0,
		"setting env should succeed");
	CHECK(
		process_config_set_stdout_fd(config, pipe_out[1]) == 0,
		"setting stdout fd should succeed");

	int exit_code;
	int spawn_result = spawn_and_wait(config, &exit_code);

	close(pipe_out[1]);

	CHECK(spawn_result == 0, "spawning with an explicit env should succeed");
	CHECK(exit_code == 0, "the spawned shell should exit cleanly");

	char output[256];

	CHECK(
		read_all_trimmed(pipe_out[0], output, sizeof(output)) == 0,
		"reading the captured env var should succeed");
	CHECK(
		strcmp(output, "hello123") == 0,
		"the child should see exactly the env this config specified");

	close(pipe_out[0]);
	process_config_destroy(config);

	return 0;
}

static int test_env_unset_inherits_parent(void)
{
	CHECK(
		setenv("EMBLA_TEST_INHERIT_VAR", "inherited456", 1) == 0,
		"setting a var in this test process's own environment "
		"should succeed");

	int pipe_out[2];

	CHECK(pipe(pipe_out) == 0, "creating the output pipe should succeed");
	set_cloexec(pipe_out[0]);

	char *argv[] = {"sh", "-c", "printf '%s' \"$EMBLA_TEST_INHERIT_VAR\"", NULL};
	ProcessConfig *config = process_config_create("/bin/sh", argv);

	CHECK(config != NULL, "process_config_create should succeed");
	CHECK(
		process_config_set_stdout_fd(config, pipe_out[1]) == 0,
		"setting stdout fd should succeed");

	int exit_code;
	int spawn_result = spawn_and_wait(config, &exit_code);

	close(pipe_out[1]);

	CHECK(spawn_result == 0, "spawning with env left unset should succeed");
	CHECK(exit_code == 0, "the spawned shell should exit cleanly");

	char output[256];

	CHECK(
		read_all_trimmed(pipe_out[0], output, sizeof(output)) == 0,
		"reading the captured env var should succeed");
	CHECK(
		strcmp(output, "inherited456") == 0,
		"leaving env unset should make the child inherit this test "
		"process's real environment");

	close(pipe_out[0]);
	process_config_destroy(config);

	return 0;
}

static int test_stdin_redirect(void)
{
	int pipe_in[2];
	int pipe_out[2];

	CHECK(pipe(pipe_in) == 0, "creating the input pipe should succeed");
	CHECK(pipe(pipe_out) == 0, "creating the output pipe should succeed");

	set_cloexec(pipe_in[1]);
	set_cloexec(pipe_out[0]);

	char *argv[] = {"sh", "-c", "cat", NULL};
	ProcessConfig *config = process_config_create("/bin/sh", argv);

	CHECK(config != NULL, "process_config_create should succeed");
	CHECK(
		process_config_set_stdin_fd(config, pipe_in[0]) == 0,
		"setting stdin fd should succeed");
	CHECK(
		process_config_set_stdout_fd(config, pipe_out[1]) == 0,
		"setting stdout fd should succeed");

	Executor *executor = executor_create();
	Process *process = process_create(1, EMBLA_ROOT_PID, 1, "test-process");

	CHECK(executor != NULL && process != NULL, "fixtures should allocate");
	CHECK(
		executor_spawn(
			executor,
			process,
			EMBLA_INVALID_HOST_PGID,
			config) == 0,
		"spawning with stdin/stdout redirected should succeed");
	CHECK(
		process_transition(process, PROCESS_READY) == 0,
		"transitioning to READY should succeed");

	close(pipe_in[0]);
	close(pipe_out[1]);

	CHECK(
		write(pipe_in[1], "ping", 4) == 4,
		"writing to the child's stdin should succeed");
	close(pipe_in[1]);

	CHECK(
		executor_wait(executor, process, NULL) == 0,
		"waiting for the child to exit should succeed");
	CHECK(
		process_get_exit_code(process) == 0,
		"the spawned shell should exit cleanly");

	char output[256];

	CHECK(
		read_all_trimmed(pipe_out[0], output, sizeof(output)) == 0,
		"reading what cat echoed back should succeed");
	CHECK(
		strcmp(output, "ping") == 0,
		"cat should have echoed back exactly what was written to "
		"the child's redirected stdin");

	close(pipe_out[0]);
	process_destroy(process);
	executor_destroy(executor);
	process_config_destroy(config);

	return 0;
}

static int test_stderr_redirect(void)
{
	int pipe_err[2];

	CHECK(pipe(pipe_err) == 0, "creating the stderr pipe should succeed");
	set_cloexec(pipe_err[0]);

	char *argv[] = {"sh", "-c", "echo err-message 1>&2", NULL};
	ProcessConfig *config = process_config_create("/bin/sh", argv);

	CHECK(config != NULL, "process_config_create should succeed");
	CHECK(
		process_config_set_stderr_fd(config, pipe_err[1]) == 0,
		"setting stderr fd should succeed");

	int exit_code;
	int spawn_result = spawn_and_wait(config, &exit_code);

	close(pipe_err[1]);

	CHECK(spawn_result == 0, "spawning with stderr redirected should succeed");
	CHECK(exit_code == 0, "the spawned shell should exit cleanly");

	char output[256];

	CHECK(
		read_all_trimmed(pipe_err[0], output, sizeof(output)) == 0,
		"reading the captured stderr should succeed");
	CHECK(
		strcmp(output, "err-message") == 0,
		"the child's stderr should have been redirected, not its "
		"stdout");

	close(pipe_err[0]);
	process_config_destroy(config);

	return 0;
}

static int test_umask_applied(void)
{
	int pipe_out[2];

	CHECK(pipe(pipe_out) == 0, "creating the output pipe should succeed");
	set_cloexec(pipe_out[0]);

	char *argv[] = {"sh", "-c", "umask", NULL};
	ProcessConfig *config = process_config_create("/bin/sh", argv);

	CHECK(config != NULL, "process_config_create should succeed");
	CHECK(
		process_config_set_umask(config, 0027) == 0,
		"setting umask should succeed");
	CHECK(
		process_config_set_stdout_fd(config, pipe_out[1]) == 0,
		"setting stdout fd should succeed");

	int exit_code;
	int spawn_result = spawn_and_wait(config, &exit_code);

	close(pipe_out[1]);

	CHECK(spawn_result == 0, "spawning with a umask should succeed");
	CHECK(exit_code == 0, "the spawned shell should exit cleanly");

	char output[256];

	CHECK(
		read_all_trimmed(pipe_out[0], output, sizeof(output)) == 0,
		"reading the captured umask output should succeed");

	long reported = strtol(output, NULL, 8);

	CHECK(
		reported == 0027,
		"the child's umask should match what this config set, "
		"parsed as octal to tolerate shells' varying 0-prefix "
		"formatting");

	close(pipe_out[0]);
	process_config_destroy(config);

	return 0;
}

static int test_credentials_self_is_a_noop(void)
{
	int pipe_out[2];

	CHECK(pipe(pipe_out) == 0, "creating the output pipe should succeed");
	set_cloexec(pipe_out[0]);

	char *argv[] = {"sh", "-c", "exit 0", NULL};
	ProcessConfig *config = process_config_create("/bin/sh", argv);

	CHECK(config != NULL, "process_config_create should succeed");
	CHECK(
		process_config_set_credentials(config, getuid(), getgid()) == 0,
		"setting credentials to this process's own uid/gid should "
		"succeed");
	CHECK(
		process_config_set_stdout_fd(config, pipe_out[1]) == 0,
		"setting stdout fd should succeed");

	int exit_code;
	int spawn_result = spawn_and_wait(config, &exit_code);

	close(pipe_out[1]);
	close(pipe_out[0]);

	CHECK(
		spawn_result == 0,
		"spawning with credentials set to this process's own "
		"identity should succeed, exercising setgid()/setuid() "
		"without requiring any actual privilege change");
	CHECK(exit_code == 0, "the spawned shell should exit cleanly");

	process_config_destroy(config);

	return 0;
}

int main(void)
{
	struct
	{
		const char *name;
		int (*fn)(void);
	} tests[] = {
		{"working_directory_applied", test_working_directory_applied},
		{"env_explicit", test_env_explicit},
		{"env_unset_inherits_parent", test_env_unset_inherits_parent},
		{"stdin_redirect", test_stdin_redirect},
		{"stderr_redirect", test_stderr_redirect},
		{"umask_applied", test_umask_applied},
		{"credentials_self_is_a_noop", test_credentials_self_is_a_noop},
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

	printf("all %zu process-config spawn tests passed\n", count);

	return EXIT_SUCCESS;
}
