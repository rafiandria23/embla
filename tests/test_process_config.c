#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static int test_create_rejects_invalid_input(void)
{
	char *argv[] = {"prog", NULL};

	CHECK(
		process_config_create(NULL, argv) == NULL,
		"NULL path must be rejected");
	CHECK(
		process_config_create("/bin/prog", NULL) == NULL,
		"NULL argv must be rejected");

	return 0;
}

static int test_defaults(void)
{
	char *argv[] = {"prog", NULL};
	ProcessConfig *config = process_config_create("/bin/prog", argv);

	CHECK(config != NULL, "create should succeed");

	CHECK(
		process_config_get_working_directory(config) == NULL,
		"working directory should default to unset");
	CHECK(
		process_config_get_env(config) == NULL,
		"env should default to unset");
	CHECK(
		process_config_get_stdin_fd(config) == -1,
		"stdin fd should default to -1 (inherit)");
	CHECK(
		process_config_get_stdout_fd(config) == -1,
		"stdout fd should default to -1 (inherit)");
	CHECK(
		process_config_get_stderr_fd(config) == -1,
		"stderr fd should default to -1 (inherit)");
	CHECK(
		process_config_has_umask(config) == 0,
		"umask should default to unset");
	CHECK(
		process_config_has_credentials(config) == 0,
		"credentials should default to unset");

	process_config_destroy(config);

	return 0;
}

static int test_path_and_argv_are_deep_copied(void)
{
	char path_buf[] = "/bin/prog";
	char arg0_buf[] = "prog";
	char arg1_buf[] = "--flag";
	char *argv[] = {arg0_buf, arg1_buf, NULL};

	ProcessConfig *config = process_config_create(path_buf, argv);

	CHECK(config != NULL, "create should succeed");
	CHECK(
		process_config_get_path(config) != path_buf,
		"stored path must be a distinct allocation, not a borrow");
	CHECK(
		strcmp(process_config_get_path(config), "/bin/prog") == 0,
		"stored path content should match");

	char *const *stored_argv = process_config_get_argv(config);

	CHECK(
		stored_argv[0] != arg0_buf && stored_argv[1] != arg1_buf,
		"argv entries must be distinct allocations, not borrows");
	CHECK(
		strcmp(stored_argv[0], "prog") == 0 &&
			strcmp(stored_argv[1], "--flag") == 0,
		"argv content should match");
	CHECK(stored_argv[2] == NULL, "argv should remain NULL-terminated");

	arg0_buf[0] = 'X';

	CHECK(
		strcmp(stored_argv[0], "prog") == 0,
		"mutating the caller's original argv must not affect the "
		"stored config -- proves the copy is deep, not a borrow");

	process_config_destroy(config);

	return 0;
}

static int test_working_directory_setter(void)
{
	char *argv[] = {"prog", NULL};
	ProcessConfig *config = process_config_create("/bin/prog", argv);

	CHECK(config != NULL, "create should succeed");
	CHECK(
		process_config_set_working_directory(config, "/tmp") == 0,
		"setting working directory should succeed");
	CHECK(
		strcmp(process_config_get_working_directory(config), "/tmp") == 0,
		"stored working directory should match");

	CHECK(
		process_config_set_working_directory(config, "/var") == 0,
		"overwriting working directory should succeed");
	CHECK(
		strcmp(process_config_get_working_directory(config), "/var") == 0,
		"overwrite should replace, not append or leak the old value");

	process_config_destroy(config);

	return 0;
}

static int test_env_setter(void)
{
	char *argv[] = {"prog", NULL};
	ProcessConfig *config = process_config_create("/bin/prog", argv);
	char *envp[] = {"KEY=VALUE", "OTHER=1", NULL};

	CHECK(config != NULL, "create should succeed");
	CHECK(
		process_config_set_env(config, envp) == 0,
		"setting env should succeed");

	char *const *stored_env = process_config_get_env(config);

	CHECK(
		strcmp(stored_env[0], "KEY=VALUE") == 0 &&
			strcmp(stored_env[1], "OTHER=1") == 0 &&
			stored_env[2] == NULL,
		"stored env should match and remain NULL-terminated");
	CHECK(
		stored_env[0] != envp[0],
		"env entries must be deep-copied, not borrowed");

	process_config_destroy(config);

	return 0;
}

static int test_stdio_fd_setters(void)
{
	char *argv[] = {"prog", NULL};
	ProcessConfig *config = process_config_create("/bin/prog", argv);

	CHECK(config != NULL, "create should succeed");

	CHECK(
		process_config_set_stdin_fd(config, -1) != 0,
		"a negative fd must be rejected");

	CHECK(process_config_set_stdin_fd(config, 3) == 0, "set stdin fd");
	CHECK(process_config_set_stdout_fd(config, 4) == 0, "set stdout fd");
	CHECK(process_config_set_stderr_fd(config, 5) == 0, "set stderr fd");

	CHECK(process_config_get_stdin_fd(config) == 3, "stdin fd readback");
	CHECK(process_config_get_stdout_fd(config) == 4, "stdout fd readback");
	CHECK(process_config_get_stderr_fd(config) == 5, "stderr fd readback");

	process_config_destroy(config);

	return 0;
}

static int test_umask_setter(void)
{
	char *argv[] = {"prog", NULL};
	ProcessConfig *config = process_config_create("/bin/prog", argv);

	CHECK(config != NULL, "create should succeed");
	CHECK(
		process_config_set_umask(config, 0022) == 0,
		"setting umask should succeed");
	CHECK(
		process_config_has_umask(config) == 1,
		"has_umask should become true after being set");
	CHECK(
		process_config_get_umask(config) == 0022,
		"stored umask should match");

	process_config_destroy(config);

	return 0;
}

static int test_credentials_setter(void)
{
	char *argv[] = {"prog", NULL};
	ProcessConfig *config = process_config_create("/bin/prog", argv);

	CHECK(config != NULL, "create should succeed");
	CHECK(
		process_config_set_credentials(config, 1000, 1000) == 0,
		"setting credentials should succeed");
	CHECK(
		process_config_has_credentials(config) == 1,
		"has_credentials should become true after being set");
	CHECK(
		process_config_get_uid(config) == 1000 &&
			process_config_get_gid(config) == 1000,
		"stored uid/gid should match");

	process_config_destroy(config);

	return 0;
}

static int test_destroy_null_is_safe(void)
{
	process_config_destroy(NULL);

	return 0;
}

int main(void)
{
	struct
	{
		const char *name;
		int (*fn)(void);
	} tests[] = {
		{"create_rejects_invalid_input", test_create_rejects_invalid_input},
		{"defaults", test_defaults},
		{"path_and_argv_are_deep_copied", test_path_and_argv_are_deep_copied},
		{"working_directory_setter", test_working_directory_setter},
		{"env_setter", test_env_setter},
		{"stdio_fd_setters", test_stdio_fd_setters},
		{"umask_setter", test_umask_setter},
		{"credentials_setter", test_credentials_setter},
		{"destroy_null_is_safe", test_destroy_null_is_safe},
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
