#include <stdio.h>
#include <stdlib.h>

#include "embla/process.h"
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

static int test_create_get_count(void)
{
	ProcessManager *manager = process_manager_create();

	CHECK(manager != NULL, "manager should allocate");
	CHECK(process_manager_count(manager) == 0, "fresh manager is empty");

	Process *root = process_manager_create_process(manager, EMBLA_ROOT_PID, 1, "root");
	Process *child = process_manager_create_process(manager, process_get_id(root), 1, "child");

	CHECK(root != NULL && child != NULL, "creating processes should succeed");
	CHECK(process_manager_count(manager) == 2, "manager should hold 2");
	CHECK(
		process_manager_get(manager, process_get_id(root)) == root,
		"lookup by id should return the same pointer");
	CHECK(
		process_manager_get(manager, EMBLA_INVALID_PID) == NULL,
		"lookup with the invalid PID sentinel must return NULL");

	CHECK(
		process_set_host_id(root, 999) == 0, "setting host id should work");
	CHECK(
		process_manager_get_by_host_id(manager, 999) == root,
		"lookup by host id should find the process");
	CHECK(
		process_manager_get_by_host_id(manager, EMBLA_INVALID_HOST_PID) ==
			NULL,
		"lookup with the invalid host PID sentinel must return NULL");

	process_manager_destroy(manager);

	return 0;
}

static int test_live_count_and_child_count(void)
{
	ProcessManager *manager = process_manager_create();

	Process *root = process_manager_create_process(manager, EMBLA_ROOT_PID, 1, "root");
	Process *c1 = process_manager_create_process(manager, process_get_id(root), 1, "c1");
	Process *c2 = process_manager_create_process(manager, process_get_id(root), 1, "c2");

	CHECK(
		root != NULL &&
			c1 != NULL &&
			c2 != NULL,
		"fixtures should exist");
	CHECK(
		process_manager_live_count(manager) == 3,
		"all 3 freshly-created processes should count as live");
	CHECK(
		process_manager_child_count(manager, process_get_id(root)) == 2,
		"root should have exactly 2 direct children");

	CHECK(process_transition(c1, PROCESS_READY) == 0, "c1 -> READY");
	CHECK(process_transition(c1, PROCESS_TERMINATED) == 0, "c1 -> TERMINATED");

	CHECK(
		process_manager_live_count(manager) == 2,
		"a terminated process must not count as live");
	CHECK(
		process_manager_child_count(manager, process_get_id(root)) == 2,
		"child_count is independent of state: a terminated but "
		"not-yet-reaped child still counts");

	process_manager_destroy(manager);

	return 0;
}

static int test_reparenting_on_termination(void)
{
	ProcessManager *manager = process_manager_create();

	Process *root = process_manager_create_process(manager, EMBLA_ROOT_PID, 1, "root");
	Process *mid = process_manager_create_process(manager, process_get_id(root), 1, "mid");
	Process *leaf = process_manager_create_process(manager, process_get_id(mid), 1, "leaf");

	CHECK(
		root != NULL &&
			mid != NULL &&
			leaf != NULL,
		"fixtures should exist");
	CHECK(
		process_get_parent_id(leaf) == process_get_id(mid),
		"leaf's parent should initially be mid");

	CHECK(
		process_manager_reparent_children(
			manager,
			process_get_id(mid),
			EMBLA_ROOT_PID) == 0,
		"reparenting mid's children to root should succeed");

	CHECK(
		process_get_parent_id(leaf) == EMBLA_ROOT_PID,
		"leaf should now be parented directly to the root sentinel");
	CHECK(
		process_manager_child_count(manager, EMBLA_ROOT_PID) == 2,
		"the root sentinel should now see both root and leaf as "
		"direct children (root was always there; leaf just joined it)");
	CHECK(
		process_manager_child_count(manager, process_get_id(root)) == 1,
		"root's own child count is unaffected: mid is still its child");
	CHECK(
		process_manager_child_count(manager, process_get_id(mid)) == 0,
		"mid should have no children left after reparenting");

	process_manager_destroy(manager);

	return 0;
}

static int test_wait_child_and_reap(void)
{
	ProcessManager *manager = process_manager_create();

	Process *root = process_manager_create_process(manager, EMBLA_ROOT_PID, 1, "root");
	Process *child = process_manager_create_process(manager, process_get_id(root), 1, "child");

	CHECK(root != NULL && child != NULL, "fixtures should exist");
	CHECK(
		process_manager_wait_child(manager, process_get_id(root)) == NULL,
		"a live child must not be reported as waitable");

	CHECK(process_transition(child, PROCESS_READY) == 0, "child -> READY");
	CHECK(
		process_transition(child, PROCESS_TERMINATED) == 0,
		"child -> TERMINATED");

	CHECK(
		process_manager_wait_child(manager, process_get_id(root)) == child,
		"a terminated child must now be waitable");

	ProcessId child_id = process_get_id(child);
	ProcessId reaped_id;

	CHECK(
		process_manager_reap_child(
			manager,
			process_get_id(root),
			&reaped_id) == 0,
		"reaping the terminated child should succeed");
	CHECK(
		reaped_id == child_id,
		"the reaped id should match the child that was reaped");
	CHECK(
		process_manager_count(manager) == 1,
		"only root should remain after reaping the child");

	process_manager_destroy(manager);

	return 0;
}

int main(void)
{
	struct
	{
		const char *name;
		int (*fn)(void);
	} tests[] = {
		{"create_get_count", test_create_get_count},
		{"live_count_and_child_count", test_live_count_and_child_count},
		{"reparenting_on_termination", test_reparenting_on_termination},
		{"wait_child_and_reap", test_wait_child_and_reap},
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
