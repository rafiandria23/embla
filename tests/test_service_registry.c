#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "embla/process_config.h"
#include "embla/service.h"
#include "embla/service_registry.h"

#define CHECK(cond, msg)                                                    \
	do                                                                      \
	{                                                                       \
		if (!(cond))                                                        \
		{                                                                   \
			fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
			return -1;                                                      \
		}                                                                   \
	} while (0)

static ProcessConfig *make_config(void)
{
	char *argv[] = {"prog", NULL};

	return process_config_create("/bin/prog", argv);
}

static int test_create_rejects_invalid_input(void)
{
	ProcessConfig *config = make_config();

	CHECK(config != NULL, "fixture config should allocate");
	CHECK(
		service_create(NULL, config) == NULL,
		"a NULL name must be rejected");

	CHECK(
		service_create("name", NULL) == NULL,
		"a NULL config must be rejected");

	process_config_destroy(config);

	return 0;
}

static int test_create_and_accessors(void)
{
	ProcessConfig *config = make_config();

	CHECK(config != NULL, "fixture config should allocate");

	Service *service = service_create("nginx", config);

	CHECK(service != NULL, "create should succeed");
	CHECK(
		strcmp(service_get_name(service), "nginx") == 0,
		"the name should read back exactly what was set");
	CHECK(
		service_get_config(service) == config,
		"the config accessor should return the exact pointer "
		"ownership was transferred for");

	service_destroy(service);

	return 0;
}

static int test_name_is_deep_copied(void)
{
	char name_buf[] = "redis";
	ProcessConfig *config = make_config();

	CHECK(config != NULL, "fixture config should allocate");

	Service *service = service_create(name_buf, config);

	CHECK(service != NULL, "create should succeed");
	CHECK(
		service_get_name(service) != name_buf,
		"the stored name must be a distinct allocation, not a "
		"borrow of the caller's buffer");

	name_buf[0] = 'X';

	CHECK(
		strcmp(service_get_name(service), "redis") == 0,
		"mutating the caller's original name buffer must not "
		"affect the stored service -- proves the copy is deep");

	service_destroy(service);

	return 0;
}

static int test_destroy_null_is_safe(void)
{
	service_destroy(NULL);

	return 0;
}

static int test_registry_register_and_get(void)
{
	ServiceRegistry *registry = service_registry_create();

	CHECK(registry != NULL, "registry create should succeed");
	CHECK(
		service_registry_count(registry) == 0,
		"a fresh registry should be empty");

	Service *nginx = service_create("nginx", make_config());
	Service *redis = service_create("redis", make_config());

	CHECK(nginx != NULL && redis != NULL, "fixtures should allocate");
	CHECK(
		service_registry_register(registry, nginx) == 0,
		"registering the first service should succeed");
	CHECK(
		service_registry_register(registry, redis) == 0,
		"registering a second, distinctly-named service should "
		"succeed");
	CHECK(
		service_registry_count(registry) == 2,
		"the registry should now hold 2 services");

	CHECK(
		service_registry_get(registry, "nginx") == nginx,
		"looking up nginx should return the exact registered "
		"pointer");
	CHECK(
		service_registry_get(registry, "redis") == redis,
		"looking up redis should return the exact registered "
		"pointer");
	CHECK(
		service_registry_get(registry, "postgres") == NULL,
		"looking up an unregistered name should return NULL");

	service_registry_destroy(registry);

	return 0;
}

static int test_registry_rejects_duplicate_name(void)
{
	ServiceRegistry *registry = service_registry_create();

	CHECK(registry != NULL, "registry create should succeed");

	Service *first = service_create("nginx", make_config());
	Service *second = service_create("nginx", make_config());

	CHECK(first != NULL && second != NULL, "fixtures should allocate");
	CHECK(
		service_registry_register(registry, first) == 0,
		"registering the first \"nginx\" should succeed");
	CHECK(
		service_registry_register(registry, second) != 0,
		"registering a SECOND service also named \"nginx\" must "
		"be rejected");
	CHECK(
		service_registry_count(registry) == 1,
		"a rejected registration must not change the registry's "
		"count");

	service_destroy(second);
	service_registry_destroy(registry);

	return 0;
}

static int test_registry_unregister_removes_and_destroys(void)
{
	ServiceRegistry *registry = service_registry_create();

	CHECK(registry != NULL, "registry create should succeed");

	Service *nginx = service_create("nginx", make_config());

	CHECK(nginx != NULL, "fixture should allocate");
	CHECK(
		service_registry_register(registry, nginx) == 0,
		"registering should succeed");
	CHECK(
		service_registry_unregister(registry, "nginx") == 0,
		"unregistering a present service should succeed");
	CHECK(
		service_registry_count(registry) == 0,
		"the registry should be empty again after unregistering "
		"its only service");
	CHECK(
		service_registry_get(registry, "nginx") == NULL,
		"the unregistered service should no longer be findable");
	CHECK(
		service_registry_unregister(registry, "nginx") != 0,
		"unregistering an already-removed name must fail, not "
		"succeed a second time");

	service_registry_destroy(registry);

	return 0;
}

static int test_registry_destroy_null_is_safe(void)
{
	service_registry_destroy(NULL);

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
			"create_rejects_invalid_input",
			test_create_rejects_invalid_input,
		},
		{"create_and_accessors", test_create_and_accessors},
		{"name_is_deep_copied", test_name_is_deep_copied},
		{"destroy_null_is_safe", test_destroy_null_is_safe},
		{"registry_register_and_get", test_registry_register_and_get},
		{
			"registry_rejects_duplicate_name",
			test_registry_rejects_duplicate_name,
		},
		{
			"registry_unregister_removes_and_destroys",
			test_registry_unregister_removes_and_destroys,
		},
		{
			"registry_destroy_null_is_safe",
			test_registry_destroy_null_is_safe,
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

	printf("all %zu unit tests passed\n", count);

	return EXIT_SUCCESS;
}
