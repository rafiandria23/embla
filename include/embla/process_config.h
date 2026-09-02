#ifndef EMBLA_PROCESS_CONFIG_H
#define EMBLA_PROCESS_CONFIG_H

#include <sys/types.h>

typedef struct ProcessConfig ProcessConfig;

ProcessConfig *process_config_create(
	const char *path,
	char *const argv[]);

void process_config_destroy(ProcessConfig *config);

const char *process_config_get_path(const ProcessConfig *config);
char *const *process_config_get_argv(const ProcessConfig *config);

int process_config_set_working_directory(
	ProcessConfig *config,
	const char *working_directory);

const char *process_config_get_working_directory(
	const ProcessConfig *config);

int process_config_set_env(
	ProcessConfig *config,
	char *const envp[]);

char *const *process_config_get_env(const ProcessConfig *config);

int process_config_set_stdin_fd(ProcessConfig *config, int fd);
int process_config_set_stdout_fd(ProcessConfig *config, int fd);
int process_config_set_stderr_fd(ProcessConfig *config, int fd);

int process_config_get_stdin_fd(const ProcessConfig *config);
int process_config_get_stdout_fd(const ProcessConfig *config);
int process_config_get_stderr_fd(const ProcessConfig *config);

int process_config_set_umask(
	ProcessConfig *config,
	mode_t umask_value);

int process_config_has_umask(const ProcessConfig *config);
mode_t process_config_get_umask(const ProcessConfig *config);

int process_config_set_credentials(
	ProcessConfig *config,
	uid_t uid,
	gid_t gid);

int process_config_has_credentials(const ProcessConfig *config);
uid_t process_config_get_uid(const ProcessConfig *config);
gid_t process_config_get_gid(const ProcessConfig *config);

#endif
