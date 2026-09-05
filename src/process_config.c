#include <stdlib.h>
#include <string.h>

#include "embla/log.h"
#include "embla/process_config.h"
#include "embla/string.h"

struct ProcessConfig
{
	char *path;
	char **argv;
	size_t argv_count;

	char *working_directory;

	char **envp;
	size_t envp_count;

	int stdin_fd;
	int stdout_fd;
	int stderr_fd;

	int has_umask;
	mode_t umask_value;

	int has_credentials;
	uid_t uid;
	gid_t gid;

	int has_memory_limit;
	rlim_t memory_limit;

	int has_cpu_limit;
	rlim_t cpu_limit;

	int has_fd_limit;
	rlim_t fd_limit;
};

static size_t count_array(char *const array[])
{
	size_t count = 0;

	while (array[count] != NULL)
	{
		count++;
	}

	return count;
}

static char **dup_string_array(char *const array[], size_t *out_count)
{
	size_t count = count_array(array);

	char **copy = calloc(count + 1, sizeof(*copy));

	if (copy == NULL)
	{
		return NULL;
	}

	for (size_t i = 0; i < count; i++)
	{
		copy[i] = embla_strdup(array[i]);

		if (copy[i] == NULL)
		{
			for (size_t j = 0; j < i; j++)
			{
				free(copy[j]);
			}

			free(copy);

			return NULL;
		}
	}

	copy[count] = NULL;

	if (out_count != NULL)
	{
		*out_count = count;
	}

	return copy;
}

static void free_string_array(char **array, size_t count)
{
	if (array == NULL)
	{
		return;
	}

	for (size_t i = 0; i < count; i++)
	{
		free(array[i]);
	}

	free(array);
}

ProcessConfig *process_config_create(
	const char *path,
	char *const argv[])
{
	if (path == NULL || argv == NULL)
	{
		return NULL;
	}

	ProcessConfig *config = calloc(1, sizeof(*config));

	if (config == NULL)
	{
		embla_log_error("failed to allocate process config");
		return NULL;
	}

	config->path = embla_strdup(path);

	if (config->path == NULL)
	{
		embla_log_error("failed to duplicate process config path");

		free(config);

		return NULL;
	}

	config->argv = dup_string_array(argv, &config->argv_count);

	if (config->argv == NULL)
	{
		embla_log_error("failed to duplicate process config argv");

		free(config->path);
		free(config);

		return NULL;
	}

	config->stdin_fd = -1;
	config->stdout_fd = -1;
	config->stderr_fd = -1;

	return config;
}

void process_config_destroy(ProcessConfig *config)
{
	if (config == NULL)
	{
		return;
	}

	free(config->path);
	free_string_array(config->argv, config->argv_count);
	free(config->working_directory);
	free_string_array(config->envp, config->envp_count);

	free(config);
}

const char *process_config_get_path(const ProcessConfig *config)
{
	if (config == NULL)
	{
		return NULL;
	}

	return config->path;
}

char *const *process_config_get_argv(const ProcessConfig *config)
{
	if (config == NULL)
	{
		return NULL;
	}

	return config->argv;
}

int process_config_set_working_directory(
	ProcessConfig *config,
	const char *working_directory)
{
	if (config == NULL || working_directory == NULL)
	{
		return -1;
	}

	char *copy = embla_strdup(working_directory);

	if (copy == NULL)
	{
		embla_log_error("failed to duplicate working directory");
		return -1;
	}

	free(config->working_directory);
	config->working_directory = copy;

	return 0;
}

const char *process_config_get_working_directory(
	const ProcessConfig *config)
{
	if (config == NULL)
	{
		return NULL;
	}

	return config->working_directory;
}

int process_config_set_env(
	ProcessConfig *config,
	char *const envp[])
{
	if (config == NULL || envp == NULL)
	{
		return -1;
	}

	size_t count;
	char **copy = dup_string_array(envp, &count);

	if (copy == NULL)
	{
		embla_log_error("failed to duplicate process config env");
		return -1;
	}

	free_string_array(config->envp, config->envp_count);
	config->envp = copy;
	config->envp_count = count;

	return 0;
}

char *const *process_config_get_env(const ProcessConfig *config)
{
	if (config == NULL)
	{
		return NULL;
	}

	return config->envp;
}

int process_config_set_stdin_fd(ProcessConfig *config, int fd)
{
	if (config == NULL || fd < 0)
	{
		return -1;
	}

	config->stdin_fd = fd;

	return 0;
}

int process_config_set_stdout_fd(ProcessConfig *config, int fd)
{
	if (config == NULL || fd < 0)
	{
		return -1;
	}

	config->stdout_fd = fd;

	return 0;
}

int process_config_set_stderr_fd(ProcessConfig *config, int fd)
{
	if (config == NULL || fd < 0)
	{
		return -1;
	}

	config->stderr_fd = fd;

	return 0;
}

int process_config_get_stdin_fd(const ProcessConfig *config)
{
	if (config == NULL)
	{
		return -1;
	}

	return config->stdin_fd;
}

int process_config_get_stdout_fd(const ProcessConfig *config)
{
	if (config == NULL)
	{
		return -1;
	}

	return config->stdout_fd;
}

int process_config_get_stderr_fd(const ProcessConfig *config)
{
	if (config == NULL)
	{
		return -1;
	}

	return config->stderr_fd;
}

int process_config_set_umask(
	ProcessConfig *config,
	mode_t umask_value)
{
	if (config == NULL)
	{
		return -1;
	}

	config->has_umask = 1;
	config->umask_value = umask_value;

	return 0;
}

int process_config_has_umask(const ProcessConfig *config)
{
	if (config == NULL)
	{
		return 0;
	}

	return config->has_umask;
}

mode_t process_config_get_umask(const ProcessConfig *config)
{
	if (config == NULL)
	{
		return 0;
	}

	return config->umask_value;
}

int process_config_set_credentials(
	ProcessConfig *config,
	uid_t uid,
	gid_t gid)
{
	if (config == NULL)
	{
		return -1;
	}

	config->has_credentials = 1;
	config->uid = uid;
	config->gid = gid;

	return 0;
}

int process_config_has_credentials(const ProcessConfig *config)
{
	if (config == NULL)
	{
		return 0;
	}

	return config->has_credentials;
}

uid_t process_config_get_uid(const ProcessConfig *config)
{
	if (config == NULL)
	{
		return 0;
	}

	return config->uid;
}

gid_t process_config_get_gid(const ProcessConfig *config)
{
	if (config == NULL)
	{
		return 0;
	}

	return config->gid;
}

int process_config_set_memory_limit(
	ProcessConfig *config,
	rlim_t bytes)
{
	if (config == NULL)
	{
		return -1;
	}

	config->has_memory_limit = 1;
	config->memory_limit = bytes;

	return 0;
}

int process_config_has_memory_limit(const ProcessConfig *config)
{
	if (config == NULL)
	{
		return 0;
	}

	return config->has_memory_limit;
}

rlim_t process_config_get_memory_limit(const ProcessConfig *config)
{
	if (config == NULL)
	{
		return 0;
	}

	return config->memory_limit;
}

int process_config_set_cpu_limit(
	ProcessConfig *config,
	rlim_t seconds)
{
	if (config == NULL)
	{
		return -1;
	}

	config->has_cpu_limit = 1;
	config->cpu_limit = seconds;

	return 0;
}

int process_config_has_cpu_limit(const ProcessConfig *config)
{
	if (config == NULL)
	{
		return 0;
	}

	return config->has_cpu_limit;
}

rlim_t process_config_get_cpu_limit(const ProcessConfig *config)
{
	if (config == NULL)
	{
		return 0;
	}

	return config->cpu_limit;
}

int process_config_set_fd_limit(
	ProcessConfig *config,
	rlim_t count)
{
	if (config == NULL)
	{
		return -1;
	}

	config->has_fd_limit = 1;
	config->fd_limit = count;

	return 0;
}

int process_config_has_fd_limit(const ProcessConfig *config)
{
	if (config == NULL)
	{
		return 0;
	}

	return config->has_fd_limit;
}

rlim_t process_config_get_fd_limit(const ProcessConfig *config)
{
	if (config == NULL)
	{
		return 0;
	}

	return config->fd_limit;
}
