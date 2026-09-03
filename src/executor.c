#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "embla/executor.h"
#include "embla/log.h"

struct Executor
{
	int placeholder;
};

Executor *executor_create(void)
{
	Executor *executor = malloc(sizeof(*executor));

	if (executor == NULL)
	{
		embla_log_error("failed to allocate executor");
		return NULL;
	}

	executor->placeholder = 0;

	return executor;
}

void executor_destroy(Executor *executor)
{
	if (executor == NULL)
	{
		return;
	}

	free(executor);
}

static pid_t executor_waitpid_retry(
	pid_t pid,
	int *wait_status,
	int options)
{
	pid_t result;

	do
	{
		result = waitpid(pid, wait_status, options);
	} while (result == -1 && errno == EINTR);

	return result;
}

static int executor_kill_and_reap(pid_t pid)
{
	if (kill(pid, SIGKILL) != 0 && errno != ESRCH)
	{
		embla_log_error("failed to kill host process");
		return -1;
	}

	int wait_status;

	pid_t result = executor_waitpid_retry(pid, &wait_status, 0);

	if (result == -1)
	{
		embla_log_error("waitpid failed while reaping killed process");
		return -1;
	}

	return 0;
}

static int executor_apply_termination(
	Process *process,
	int wait_status)
{
	if (WIFEXITED(wait_status))
	{
		if (process_set_exit_code(process, WEXITSTATUS(wait_status)) != 0)
		{
			embla_log_error("failed to set process exit code");
			return -1;
		}
	}
	else if (WIFSIGNALED(wait_status))
	{
		if (process_set_term_signal(process, WTERMSIG(wait_status)) != 0)
		{
			embla_log_error("failed to set process termination signal");
			return -1;
		}
	}

	if (process_transition(process, PROCESS_TERMINATED) != 0)
	{
		embla_log_error("failed to transition process to TERMINATED");
		return -1;
	}

	return 0;
}

int executor_spawn(
	Executor *executor,
	Process *process,
	HostProcessGroupId target_host_group_id,
	const char *path,
	char *const argv[])
{
	if (
		executor == NULL ||
		process == NULL ||
		path == NULL ||
		argv == NULL)
	{
		return -1;
	}

	pid_t pid = fork();

	if (pid < 0)
	{
		embla_log_error("fork failed");
		return -1;
	}

	if (pid == 0)
	{
		if (setpgid(0, target_host_group_id) != 0)
		{
			_exit(126);
		}

		execv(path, argv);

		// If execv() returns, it failed
		_exit(127);
	}

	if (
		setpgid(pid, target_host_group_id) != 0 &&
		errno != EACCES)
	{
		embla_log_error("failed to set host process group");
		executor_kill_and_reap(pid);
		return -1;
	}

	if (process_set_host_id(process, pid) != 0)
	{
		embla_log_error("failed to assign host pid");
		executor_kill_and_reap(pid);
		return -1;
	}

	return 0;
}

int executor_terminate(Executor *executor, Process *process)
{
	if (executor == NULL || process == NULL)
	{
		return -1;
	}

	HostProcessId host_id = process_get_host_id(process);

	if (host_id == EMBLA_INVALID_HOST_PID)
	{
		return -1;
	}

	return executor_kill_and_reap(host_id);
}

int executor_wait(Executor *executor, Process *process, int *status)
{
	if (executor == NULL || process == NULL)
	{
		return -1;
	}

	HostProcessId host_id = process_get_host_id(process);

	if (host_id == EMBLA_INVALID_HOST_PID)
	{
		return -1;
	}

	int wait_status;

	pid_t result = executor_waitpid_retry(host_id, &wait_status, 0);

	if (result == -1)
	{
		embla_log_error("waitpid failed");
		return -1;
	}

	if (executor_apply_termination(process, wait_status) != 0)
	{
		return -1;
	}

	if (status != NULL)
	{
		*status = wait_status;
	}

	return 0;
}

int executor_poll(Executor *executor, Process *process)
{
	if (executor == NULL || process == NULL)
	{
		return -1;
	}

	HostProcessId host_id = process_get_host_id(process);

	if (host_id == EMBLA_INVALID_HOST_PID)
	{
		return -1;
	}

	int wait_status = 0;

	pid_t result = executor_waitpid_retry(host_id, &wait_status, WNOHANG);

	if (result == -1)
	{
		embla_log_error("waitpid failed");
		return -1;
	}

	// The child is still running
	if (result == 0)
	{
		return 0;
	}

	if (!WIFEXITED(wait_status) && !WIFSIGNALED(wait_status))
	{
		return 0;
	}

	if (executor_apply_termination(process, wait_status) != 0)
	{
		return -1;
	}

	return 1;
}

int executor_poll_any(
	Executor *executor,
	HostProcessId *host_id,
	int *status)
{
	if (
		executor == NULL ||
		host_id == NULL ||
		status == NULL)
	{
		return -1;
	}

	int wait_status = 0;

	pid_t result = executor_waitpid_retry(
		-1,
		&wait_status,
		WNOHANG | WUNTRACED | WCONTINUED);

	if (result == -1)
	{
		if (errno == ECHILD)
		{
			return 0;
		}

		embla_log_error("waitpid failed");

		return -1;
	}

	if (result == 0)
	{
		return 0;
	}

	*host_id = (HostProcessId)result;
	*status = wait_status;

	return 1;
}

int executor_signal(
	Executor *executor,
	Process *process,
	int signal)
{
	if (
		executor == NULL ||
		process == NULL)
	{
		return -1;
	}

	HostProcessId host_id = process_get_host_id(process);

	if (host_id == EMBLA_INVALID_HOST_PID)
	{
		return -1;
	}

	if (kill(host_id, signal) != 0)
	{
		embla_log_error("failed to send signal");
		return -1;
	}

	return 0;
}

int executor_signal_group(
	Executor *executor,
	HostProcessGroupId host_group_id,
	int signal)
{
	if (executor == NULL)
	{
		return -1;
	}

	if (host_group_id == EMBLA_INVALID_HOST_PGID)
	{
		return -1;
	}

	if (kill(-host_group_id, signal) != 0)
	{
		embla_log_error("failed to send signal to process group");
		return -1;
	}

	return 0;
}
