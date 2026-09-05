#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "embla/executor.h"
#include "embla/platform.h"
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
			executor,
			process,
			EMBLA_INVALID_HOST_PGID,
			config) != 0)
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

static int test_memory_limit_too_tight_for_even_sh_to_load(void)
{
	char *argv[] = {"sh", "-c", "exit 0", NULL};
	ProcessConfig *config = process_config_create("/bin/sh", argv);

	CHECK(config != NULL, "config create");
	CHECK(
		process_config_set_memory_limit(config, 512 * 1024) == 0,
		"set a 512KB memory limit");

	int exit_code = -1;
	int spawn_result = spawn_and_wait(config, &exit_code);

	CHECK(
		spawn_result == 0,
		"spawning should succeed at the Embla level "
		"-- fork()/exec() themselves succeed; the limit's effect "
		"shows up as the loaded program's own startup failure, "
		"reflected in its exit code, not as a spawn failure");
	CHECK(
		exit_code != 0,
		"512KB should be far too tight for sh's own dynamic linker "
		"to load at all, on any reasonable system -- the exact "
		"failure exit code is shell/libc-specific and deliberately "
		"not asserted here");

	process_config_destroy(config);

	return 0;
}

static int test_memory_limit_comfortable_still_works(void)
{
	char *argv[] = {"sh", "-c", "exit 0", NULL};
	ProcessConfig *config = process_config_create("/bin/sh", argv);

	CHECK(config != NULL, "config create");
	CHECK(
		process_config_set_memory_limit(config, 512 * 1024 * 1024) == 0,
		"setting a memory limit on the config itself always "
		"succeeds regardless of platform support -- ProcessConfig "
		"just records the request; whether it can actually be "
		"enforced is executor_spawn()'s concern, checked below");

	int exit_code = -1;
	int spawn_result = spawn_and_wait(config, &exit_code);

	CHECK(spawn_result == 0, "spawning should succeed");

	if (platform_memory_limit_supported())
	{
		CHECK(
			exit_code == 0,
			"on a platform that supports memory limiting (Linux), "
			"a generous 512MB limit should not interfere with a "
			"normal sh invocation at all -- proves the previous "
			"test's failure was genuinely caused by the tight "
			"limit, not some unrelated environmental issue");
	}
	else
	{
		CHECK(
			exit_code == 128,
			"on a platform that does NOT support memory limiting "
			"(confirmed: Darwin rejects RLIMIT_AS unconditionally, "
			"for any requested value), even a generous request "
			"must fail loudly and predictably via this project's "
			"own memory-limit-failure exit code -- never silently "
			"succeed as an unenforced no-op, which would be far "
			"more dangerous than an honest, visible failure");
	}

	process_config_destroy(config);

	return 0;
}

static int test_cpu_limit_kills_a_busy_loop(void)
{
	char *argv[] = {"sh", "-c", "while : ; do : ; done", NULL};
	ProcessConfig *config = process_config_create("/bin/sh", argv);

	CHECK(config != NULL, "config create");
	CHECK(
		process_config_set_cpu_limit(config, 1) == 0,
		"set a 1-second CPU limit");

	Executor *executor = executor_create();
	Process *process = process_create(1, EMBLA_ROOT_PID, 1, "cpu-hog");

	CHECK(executor != NULL && process != NULL, "fixtures should allocate");
	CHECK(
		executor_spawn(
			executor, process, EMBLA_INVALID_HOST_PGID, config) == 0,
		"spawning the busy loop should succeed");
	CHECK(
		process_transition(process, PROCESS_READY) == 0,
		"transitioning to READY should succeed");

	struct timespec start;
	clock_gettime(CLOCK_MONOTONIC, &start);

	CHECK(
		executor_wait(executor, process, NULL) == 0,
		"waiting for the CPU-limited child to terminate should "
		"succeed -- this call blocks until the kernel actually "
		"enforces the limit; if RLIMIT_CPU weren't being applied "
		"at all, this would hang indefinitely instead of returning");

	struct timespec end;
	clock_gettime(CLOCK_MONOTONIC, &end);

	double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

	CHECK(
		elapsed < 5.0,
		"the limit should terminate the child within a few seconds "
		"of the configured 1-second CPU budget, not run indefinitely");
	CHECK(
		process_get_term_signal(process) > 0,
		"the child should be terminated by SOME real signal -- "
		"RLIMIT_CPU enforcement is always signal-based, but which "
		"exact signal fires (SIGKILL vs SIGXCPU) is not portable "
		"across systems, so only that a real signal was delivered "
		"is asserted, not which one");

	process_destroy(process);
	executor_destroy(executor);
	process_config_destroy(config);

	return 0;
}

static int test_fd_limit_too_tight_for_even_true_to_load(void)
{
	char *argv[] = {"true", NULL};
	ProcessConfig *config = process_config_create("/bin/true", argv);

	CHECK(config != NULL, "config create");
	CHECK(
		process_config_set_fd_limit(config, 3) == 0,
		"set an fd limit of 3 -- only stdin/stdout/stderr, no room "
		"for the dynamic linker to open a single shared library");

	int exit_code = -1;
	int spawn_result = spawn_and_wait(config, &exit_code);

	CHECK(spawn_result == 0, "spawning should succeed at the Embla level");
	CHECK(
		exit_code == 127,
		"a limit of 3 fds should be too tight for even /bin/true's "
		"own dynamic linker to load a shared library, reflected in "
		"the conventional dynamic-linker exit code 127");

	process_config_destroy(config);

	return 0;
}

static int test_fd_limit_comfortable_allows_opening_new_fds(void)
{
	pid_t pid = fork();

	CHECK(pid >= 0, "fork should succeed");

	if (pid == 0)
	{
		struct rlimit lim = {20, 20};

		if (setrlimit(RLIMIT_NOFILE, &lim) != 0)
		{
			_exit(1);
		}

		int fd = open("/dev/null", O_RDONLY);

		_exit(fd >= 0 ? 0 : 1);
	}

	int status;
	waitpid(pid, &status, 0);

	CHECK(
		WIFEXITED(status) && WEXITSTATUS(status) == 0,
		"a generous NOFILE=20 limit (17 fds beyond the standard "
		"3) should comfortably allow opening at least one "
		"additional fd -- proves the kernel mechanism itself "
		"works correctly, independent of any specific program's "
		"own loader behavior");

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
			"memory_limit_too_tight_for_even_sh_to_load",
			test_memory_limit_too_tight_for_even_sh_to_load,
		},
		{
			"memory_limit_comfortable_still_works",
			test_memory_limit_comfortable_still_works,
		},
		{"cpu_limit_kills_a_busy_loop", test_cpu_limit_kills_a_busy_loop},
		{
			"fd_limit_too_tight_for_even_true_to_load",
			test_fd_limit_too_tight_for_even_true_to_load,
		},
		{
			"fd_limit_comfortable_allows_opening_new_fds",
			test_fd_limit_comfortable_allows_opening_new_fds,
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

	printf("all %zu resource limit tests passed\n", count);

	return EXIT_SUCCESS;
}
