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

int executor_spawn(
	Executor *executor,
	Process *process,
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
		execv(path, argv);

		// If execv() returns, it failed
		_exit(127);
	}

	// Parent process
	if (process_set_host_id(process, pid) != 0)
	{
		embla_log_error("failed to assign host pid");
		kill(pid, SIGKILL);

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

	if (kill(host_id, SIGKILL) != 0)
	{
		if (errno != ESRCH)
		{
			embla_log_error("failed to terminate process");
			return -1;
		}
	}

	int wait_status;
	pid_t result;

	do
	{
		result = waitpid(host_id, &wait_status, 0);
	} while (result == -1 && errno == EINTR);

	if (result == -1)
	{
		embla_log_error("waitpid failed while terminating process");
		return -1;
	}

	return 0;
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

	pid_t result;

	do
	{
		result = waitpid(host_id, &wait_status, 0);
	} while (result == -1 && errno == EINTR);

	if (result == -1)
	{
		embla_log_error("waitpid failed");
		return -1;
	}

	if (WIFEXITED(wait_status))
	{
		if (process_set_exit_code(process, WEXITSTATUS(wait_status)) != 0)
		{
			return -1;
		}
	}
	else if (WIFSIGNALED(wait_status))
	{
		if (process_set_term_signal(process, WTERMSIG(wait_status)) != 0)
		{
			return -1;
		}
	}

	if (process_transition(process, PROCESS_TERMINATED) != 0)
	{
		embla_log_error("failed to transition process to terminate");
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
	pid_t result;

	do
	{
		result = waitpid(host_id, &wait_status, WNOHANG);
	} while (result == -1 && errno == EINTR);

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

	// The child has changed state
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
	else
	{
		return 0;
	}

	if (process_transition(process, PROCESS_TERMINATED) != 0)
	{
		embla_log_error("failed to transition process to terminated");
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
	pid_t result;

	do
	{
		result = waitpid(
			-1,
			&wait_status,
			WNOHANG | WUNTRACED | WCONTINUED);
	} while (result == -1 && errno == EINTR);

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
