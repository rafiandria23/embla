#include <stdio.h>
#include <stdlib.h>

#include "embla/process.h"
#include "embla/process_group.h"
#include "embla/process_group_manager.h"

#define CHECK(cond, msg)                                                    \
	do                                                                      \
	{                                                                       \
		if (!(cond))                                                        \
		{                                                                   \
			fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
			return -1;                                                      \
		}                                                                   \
	} while (0)

static int test_create_and_destroy(void)
{
	ProcessGroup *group = process_group_create(1);

	CHECK(group != NULL, "process_group_create should succeed");
	CHECK(
		process_group_get_id(group) == 1,
		"group id should match what was passed to create()");
	CHECK(
		process_group_get_host_id(group) == EMBLA_INVALID_HOST_PGID,
		"a freshly created group must report an invalid host PGID "
		"until process_group_set_host_id() is called");
	CHECK(
		process_group_count(group) == 0,
		"a freshly created group should have no members");

	process_group_destroy(group);

	CHECK(
		process_group_create(EMBLA_INVALID_PGID) == NULL,
		"creating a group with the invalid PGID sentinel must fail");

	return 0;
}

static int test_membership(void)
{
	ProcessGroup *group = process_group_create(1);

	Process *a = process_create(1, EMBLA_ROOT_PID, 1, "a");
	Process *b = process_create(2, EMBLA_ROOT_PID, 1, "b");
	Process *wrong_group = process_create(3, EMBLA_ROOT_PID, 2, "wrong");

	CHECK(
		group != NULL &&
			a != NULL &&
			b != NULL &&
			wrong_group != NULL,
		"fixtures should allocate");

	CHECK(
		process_group_add(group, a) == 0,
		"adding a process whose group_id matches should succeed");
	CHECK(
		process_group_count(group) == 1,
		"count should reflect the single added member");

	CHECK(
		process_group_add(group, a) != 0,
		"adding the same process twice must be rejected");
	CHECK(
		process_group_count(group) == 1,
		"a rejected duplicate add must not change the count");

	CHECK(
		process_group_add(group, wrong_group) != 0,
		"adding a process whose group_id does not match must be rejected");

	CHECK(
		process_group_add(group, b) == 0,
		"adding a second, distinct, matching process should succeed");
	CHECK(process_group_count(group) == 2, "count should now be 2");

	CHECK(
		process_group_remove(group, a) == 0,
		"removing a present member should succeed");
	CHECK(process_group_count(group) == 1, "count should drop back to 1");

	CHECK(
		process_group_remove(group, a) != 0,
		"removing an already-removed member must fail");

	CHECK(
		process_group_remove(group, b) == 0,
		"removing the last member should succeed");
	CHECK(process_group_count(group) == 0, "group should now be empty");

	process_destroy(a);
	process_destroy(b);
	process_destroy(wrong_group);
	process_group_destroy(group);

	return 0;
}

static int test_host_id_roundtrip(void)
{
	ProcessGroup *group = process_group_create(1);

	CHECK(group != NULL, "fixture should allocate");

	CHECK(
		process_group_set_host_id(group, 4242) == 0,
		"setting a host PGID should succeed");
	CHECK(
		process_group_get_host_id(group) == 4242,
		"the host PGID read back should match what was set");

	process_group_destroy(group);

	return 0;
}

static int test_manager_lifecycle(void)
{
	ProcessGroupManager *manager = process_group_manager_create();

	CHECK(manager != NULL, "manager should allocate");
	CHECK(
		process_group_manager_count(manager) == 0,
		"a fresh manager should own no groups");

	ProcessGroup *g1 = process_group_manager_create_group(manager);
	ProcessGroup *g2 = process_group_manager_create_group(manager);

	CHECK(g1 != NULL && g2 != NULL, "group creation should succeed");
	CHECK(
		process_group_get_id(g1) != process_group_get_id(g2),
		"distinct groups must get distinct ids");
	CHECK(
		process_group_manager_count(manager) == 2,
		"manager should now own two groups");

	ProcessGroupId g1_id = process_group_get_id(g1);

	CHECK(
		process_group_manager_get(manager, g1_id) == g1,
		"lookup by id should return the same pointer");
	CHECK(
		process_group_manager_get(manager, EMBLA_INVALID_PGID) == NULL,
		"lookup with the invalid PGID sentinel must return NULL");

	Process *member = process_create(1, EMBLA_ROOT_PID, g1_id, "member");

	CHECK(member != NULL, "fixture should allocate");
	CHECK(
		process_group_add(g1, member) == 0,
		"adding a member to g1 should succeed");

	CHECK(
		process_group_manager_destroy_group(manager, g1_id) != 0,
		"destroying a non-empty group must be rejected");
	CHECK(
		process_group_manager_count(manager) == 2,
		"a rejected destroy must not change the manager's count");

	CHECK(
		process_group_remove(g1, member) == 0,
		"removing the member should succeed");
	CHECK(
		process_group_manager_destroy_group(manager, g1_id) == 0,
		"destroying an empty group should now succeed");
	CHECK(
		process_group_manager_count(manager) == 1,
		"manager should own one group after destroying the other");

	process_destroy(member);
	process_group_manager_destroy(manager);

	return 0;
}

int main(void)
{
	struct
	{
		const char *name;
		int (*fn)(void);
	} tests[] = {
		{"create_and_destroy", test_create_and_destroy},
		{"membership", test_membership},
		{"host_id_roundtrip", test_host_id_roundtrip},
		{"manager_lifecycle", test_manager_lifecycle},
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
