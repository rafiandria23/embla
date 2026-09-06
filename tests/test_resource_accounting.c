#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "embla/embla.h"
#include "embla/executor.h"
#include "embla/process.h"
#include "embla/process_config.h"
#include "embla/process_manager.h"

#define CHECK(cond, msg)                                                    \
	do                                                                      \
	{                                                                       \
		if (!(cond))                                                        \
		{                                                                   \
			fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
			return -1;                                                      \
		}                                                                   \
	} while (0)

static int test_cpu_time_captured_via_executor_wait(void)
{
	char *argv[] = {
		"sh", "-c",
		"i=0; while [ $i -lt 2000000 ]; do i=$((i+1)); done",
		NULL};
	ProcessConfig *config = process_config_create("/bin/sh", argv);

	CHECK(config != NULL, "config create should succeed");

	Executor *executor = executor_create();
	Process *process = process_create(1, EMBLA_ROOT_PID, 1, "cpu-burner");

	CHECK(
		executor != NULL && process != NULL,
		"fixtures should allocate");
	CHECK(
		executor_spawn(
			executor,
			process,
			EMBLA_INVALID_HOST_PGID,
			config) == 0,
		"spawning should succeed");
	CHECK(
		process_transition(process, PROCESS_READY) == 0,
		"transitioning to READY should succeed");
	CHECK(
		executor_wait(executor, process, NULL) == 0,
		"waiting for the loop to finish should succeed");

	double user_seconds = process_get_cpu_user_seconds(process);

	CHECK(
		user_seconds > 0.0,
		"a 2-million-iteration shell arithmetic loop should "
		"consume SOME real, measurable CPU time -- not the -1.0 "
		"unset sentinel, not a bogus 0");
	CHECK(
		user_seconds < 30.0,
		"the reported CPU time should be within a generous sanity "
		"ceiling, ruling out a garbage/corrupted value from a "
		"units or capture bug");

	process_destroy(process);
	executor_destroy(executor);
	process_config_destroy(config);

	return 0;
}

static int test_max_rss_captured_via_poll_any_path(void)
{
	Embla *embla = embla_create();

	CHECK(embla != NULL, "embla_create should succeed");

	char *argv[] = {"sh", "-c", "exit 0", NULL};
	ProcessConfig *config = process_config_create("/bin/sh", argv);

	CHECK(config != NULL, "config create should succeed");

	Process *process = embla_spawn(embla, "trivial", config);

	CHECK(process != NULL, "spawning should succeed");

	while (process_get_state(process) != PROCESS_TERMINATED)
	{
		HostProcessId host_id;
		int wait_status;
		struct rusage usage;

		int result = executor_poll_any(
			embla_executor(embla),
			&host_id,
			&wait_status,
			&usage);

		CHECK(result >= 0, "polling should not fail");

		if (result == 0)
		{
			struct timespec delay = {
				.tv_sec = 0,
				.tv_nsec = 1000000};

			nanosleep(&delay, NULL);

			continue;
		}

		Process *event_process = process_manager_get_by_host_id(
			embla_process_manager(embla),
			host_id);

		CHECK(
			event_process != NULL,
			"the event's process should be found");

		if (WIFEXITED(wait_status))
		{
			CHECK(
				process_set_exit_code(
					event_process, WEXITSTATUS(wait_status)) == 0,
				"setting exit code should succeed");
		}
		else if (WIFSIGNALED(wait_status))
		{
			CHECK(
				process_set_term_signal(
					event_process, WTERMSIG(wait_status)) == 0,
				"setting term signal should succeed");
		}
		else
		{
			continue;
		}

		CHECK(
			process_transition(event_process, PROCESS_TERMINATED) == 0,
			"transitioning to TERMINATED should succeed");
		CHECK(
			executor_apply_rusage(event_process, &usage) == 0,
			"applying the captured rusage should succeed -- this "
			"is the exact same call embla_handle_child_event() "
			"makes internally");
	}

	long max_rss = process_get_max_rss_bytes(process);

	CHECK(
		max_rss >= 100000,
		"a real shell invocation's peak resident memory should be "
		"at least 100KB on any platform -- a value below this "
		"would indicate a units bug (e.g. Linux's KB-to-bytes "
		"normalization not actually happening)");
	CHECK(
		max_rss <= 1000000000,
		"a trivial shell invocation's peak resident memory should "
		"be nowhere near 1GB on any platform -- a value above "
		"this would indicate the opposite units bug (e.g. "
		"multiplying an already-bytes value by 1024 again)");

	ProcessId reaped_id;

	CHECK(
		embla_reap_child(
			embla,
			EMBLA_ROOT_PID,
			&reaped_id) == 0,
		"reaping the now-terminated process should succeed");

	process_config_destroy(config);
	embla_destroy(embla);

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
			"cpu_time_captured_via_executor_wait",
			test_cpu_time_captured_via_executor_wait,
		},
		{
			"max_rss_captured_via_poll_any_path",
			test_max_rss_captured_via_poll_any_path,
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

	printf("all %zu resource accounting tests passed\n", count);

	return EXIT_SUCCESS;
}
