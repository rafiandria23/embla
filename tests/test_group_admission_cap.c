#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>

#include "embla/embla.h"
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

static int test_group_admission_cap_rejects_third_spawn(void)
{
	Embla *embla = embla_create();

	CHECK(embla != NULL, "embla_create should succeed");

	char *argv[] = {"sleep", "30", NULL};
	ProcessConfig *config = process_config_create("/bin/sleep", argv);

	CHECK(config != NULL, "process_config_create should succeed");

	Process *first = embla_spawn(embla, "first", config);

	CHECK(first != NULL, "spawning the first (root) process should succeed");

	ProcessGroup *group = process_group_manager_get(
		embla_process_group_manager(embla),
		process_get_group_id(first));

	CHECK(group != NULL, "the group should be findable");
	CHECK(
		process_group_set_max_members(group, 2) == 0,
		"capping the group at 2 members should succeed -- set "
		"AFTER the first spawn, using the group handle already "
		"available from it, with no change needed to embla_spawn's "
		"own signature");

	Process *second = embla_spawn_child(embla, first, "second", config);

	CHECK(
		second != NULL,
		"the second spawn should succeed -- the group has 1 "
		"member so far, still below the cap of 2");
	CHECK(
		process_group_count(group) == 2,
		"the group should now have exactly 2 members");

	Process *third = embla_spawn_child(embla, first, "third", config);

	CHECK(
		third == NULL,
		"the third spawn must be rejected -- the group is already "
		"at its configured cap of 2, and this is an ADMISSION "
		"policy Embla itself enforces, not a kernel-level "
		"restriction, so it must be checked before any host "
		"process is even forked");
	CHECK(
		process_group_count(group) == 2,
		"a rejected spawn must not have changed the group's "
		"membership count at all -- confirms the admission check "
		"fires before any allocation, with nothing to roll back");

	CHECK(embla_kill(embla, first) == 0, "killing first should succeed");
	CHECK(embla_kill(embla, second) == 0, "killing second should succeed");

	CHECK(
		embla_run(embla) == 0,
		"running to drain and auto-reap both processes should "
		"succeed");

	CHECK(
		process_manager_count(embla_process_manager(embla)) == 0,
		"process manager should be empty after cleanup");
	CHECK(
		process_group_manager_count(embla_process_group_manager(embla)) == 0,
		"the now-empty group should have been cleaned up "
		"automatically");

	process_config_destroy(config);
	embla_destroy(embla);

	return 0;
}

int main(void)
{
	printf("-- group_admission_cap_rejects_third_spawn\n");

	if (test_group_admission_cap_rejects_third_spawn() != 0)
	{
		fprintf(stderr, "group admission cap test failed\n");
		return EXIT_FAILURE;
	}

	printf("group admission cap test passed\n");

	return EXIT_SUCCESS;
}
