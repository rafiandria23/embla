#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "embla/embla.h"
#include "embla/log.h"
#include "embla/process.h"
#include "embla/process_config.h"
#include "embla/process_group.h"
#include "embla/process_group_manager.h"
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

static int wait_for_group_state(
	Embla *embla,
	Process **members,
	size_t member_count,
	ProcessState expected_state)
{
	for (;;)
	{
		HostProcessId host_id;
		int wait_status;

		int result = executor_poll_any(
			embla_executor(embla),
			&host_id,
			&wait_status);

		if (result < 0)
		{
			return -1;
		}

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

		if (event_process == NULL)
		{
			return -1;
		}

		if (WIFSTOPPED(wait_status))
		{
			if (process_transition(event_process, PROCESS_STOPPED) != 0)
			{
				return -1;
			}
		}
		else if (WIFCONTINUED(wait_status))
		{
			if (process_transition(event_process, PROCESS_RUNNING) != 0)
			{
				return -1;
			}
		}
		else if (WIFEXITED(wait_status) || WIFSIGNALED(wait_status))
		{
			return -1;
		}

		int all_matched = 1;

		for (size_t i = 0; i < member_count; i++)
		{
			if (process_get_state(members[i]) != expected_state)
			{
				all_matched = 0;
				break;
			}
		}

		if (all_matched)
		{
			return 0;
		}
	}
}

static int drain_terminations(
	Embla *embla,
	size_t expected_count)
{
	size_t terminated = 0;

	while (terminated < expected_count)
	{
		HostProcessId host_id;
		int wait_status;

		int result = executor_poll_any(
			embla_executor(embla),
			&host_id,
			&wait_status);

		if (result < 0)
		{
			return -1;
		}

		if (result == 0)
		{
			struct timespec delay = {
				.tv_sec = 0,
				.tv_nsec = 1000000};

			nanosleep(&delay, NULL);

			continue;
		}

		Process *process = process_manager_get_by_host_id(
			embla_process_manager(embla),
			host_id);

		if (process == NULL)
		{
			return -1;
		}

		if (WIFSIGNALED(wait_status))
		{
			if (
				process_set_term_signal(
					process,
					WTERMSIG(wait_status)) != 0)
			{
				return -1;
			}
		}
		else if (WIFEXITED(wait_status))
		{
			if (
				process_set_exit_code(
					process,
					WEXITSTATUS(wait_status)) != 0)
			{
				return -1;
			}
		}
		else
		{
			continue;
		}

		if (process_transition(process, PROCESS_TERMINATED) != 0)
		{
			return -1;
		}

		if (scheduler_remove(embla_scheduler(embla), process) != 0)
		{
			return -1;
		}

		terminated++;
	}

	return 0;
}

static int run_lifecycle_test(void)
{
	Embla *embla = embla_create();

	CHECK(embla != NULL, "embla_create should succeed");

	char *sleep_argv[] = {"sleep", "30", NULL};

	ProcessConfig *config = process_config_create("/bin/sleep", sleep_argv);

	CHECK(config != NULL, "creating the shared process config should succeed");

	Process *parent = embla_spawn(embla, "parent", config);

	CHECK(parent != NULL, "spawning the root-owned parent should succeed");

	Process *first_child = embla_spawn_child(embla, parent, "first-child", config);
	Process *second_child = embla_spawn_child(embla, parent, "second-child", config);

	CHECK(first_child != NULL, "spawning first_child should succeed");
	CHECK(second_child != NULL, "spawning second_child should succeed");

	ProcessGroupManager *group_manager = embla_process_group_manager(embla);
	ProcessGroupId group_id = process_get_group_id(parent);
	ProcessGroup *group = process_group_manager_get(group_manager, group_id);

	CHECK(group != NULL, "the group parent belongs to should be findable");
	CHECK(
		process_group_count(group) == 3,
		"the group should have exactly 3 members");
	CHECK(
		process_get_group_id(first_child) == group_id && process_get_group_id(second_child) == group_id,
		"both children must share the parent's logical group");

	HostProcessGroupId host_pgid = process_group_get_host_id(group);

	CHECK(
		host_pgid != EMBLA_INVALID_HOST_PGID,
		"the group must have a real host PGID after spawning");

	HostProcessId leader_host_id = process_get_host_id(parent);

	CHECK(
		(HostProcessGroupId)leader_host_id == host_pgid,
		"the group leader's host PID must equal the group's host PGID");

	Process *members[3] = {
		parent,
		first_child,
		second_child};

	for (int i = 0; i < 3; i++)
	{
		HostProcessId member_host_id = process_get_host_id(members[i]);
		pid_t actual_pgid = getpgid(member_host_id);

		CHECK(
			actual_pgid != -1,
			"getpgid() should succeed for a live member");
		CHECK(
			(HostProcessGroupId)actual_pgid == host_pgid,
			"the kernel's view of this member's PGID must match "
			"the group's recorded host PGID");
	}

	CHECK(
		embla_stop_group(embla, group) == 0,
		"embla_stop_group should succeed");
	CHECK(
		wait_for_group_state(embla, members, 3, PROCESS_STOPPED) == 0,
		"all 3 group members should reach STOPPED after a "
		"group-wide SIGSTOP");

	CHECK(
		embla_continue_group(embla, group) == 0,
		"embla_continue_group should succeed");
	CHECK(
		wait_for_group_state(embla, members, 3, PROCESS_RUNNING) == 0,
		"all 3 group members should reach RUNNING after a "
		"group-wide SIGCONT");

	CHECK(
		embla_kill_group(embla, group) == 0,
		"embla_kill_group should succeed");
	CHECK(
		drain_terminations(embla, 3) == 0,
		"all 3 group members should terminate via SIGKILL");

	ProcessId parent_id = process_get_id(parent);

	for (;;)
	{
		Process *child = process_manager_wait_child(
			embla_process_manager(embla),
			parent_id);

		if (child == NULL)
		{
			break;
		}

		ProcessId reaped_id;

		CHECK(
			embla_reap_child(embla, parent_id, &reaped_id) == 0,
			"reaping a terminated child of parent should succeed");
	}

	Process *terminated_root_child = process_manager_wait_child(
		embla_process_manager(embla),
		EMBLA_ROOT_PID);

	CHECK(
		terminated_root_child == parent,
		"the terminated root-owned child should be parent itself");

	ProcessId reaped_root_id;

	CHECK(
		embla_reap_child(embla, EMBLA_ROOT_PID, &reaped_root_id) == 0,
		"reaping the root-owned parent should succeed");

	CHECK(
		process_manager_count(embla_process_manager(embla)) == 0,
		"process manager should be empty after reaping everything");
	CHECK(
		process_group_manager_count(group_manager) == 0,
		"the now-empty group must have been cleaned up automatically");

	process_config_destroy(config);
	embla_destroy(embla);

	return 0;
}

int main(void)
{
	if (run_lifecycle_test() != 0)
	{
		fprintf(stderr, "process-group lifecycle test failed\n");
		return EXIT_FAILURE;
	}

	printf("process-group lifecycle test passed\n");

	return EXIT_SUCCESS;
}
