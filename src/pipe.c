#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "embla/log.h"
#include "embla/pipe.h"

#define PIPE_CLOSED_FD (-1)

struct Pipe
{
	int read_fd;
	int write_fd;
};

static int pipe_set_fd_cloexec(int fd)
{
	int flags = fcntl(fd, F_GETFD);

	if (flags == -1)
	{
		return -1;
	}

	return fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
}

static int pipe_clear_fd_cloexec(int fd)
{
	int flags = fcntl(fd, F_GETFD);

	if (flags == -1)
	{
		return -1;
	}

	return fcntl(fd, F_SETFD, flags & ~FD_CLOEXEC);
}

Pipe *pipe_create(void)
{
	int fds[2];

	if (pipe(fds) != 0)
	{
		embla_log_error("failed to create pipe");
		return NULL;
	}

	if (
		pipe_set_fd_cloexec(fds[0]) != 0 ||
		pipe_set_fd_cloexec(fds[1]) != 0)
	{
		embla_log_error("failed to set pipe close-on-exec");

		close(fds[0]);
		close(fds[1]);

		return NULL;
	}

	Pipe *pipe = malloc(sizeof(*pipe));

	if (pipe == NULL)
	{
		embla_log_error("failed to allocate pipe");

		close(fds[0]);
		close(fds[1]);

		return NULL;
	}

	pipe->read_fd = fds[0];
	pipe->write_fd = fds[1];

	return pipe;
}

void pipe_destroy(Pipe *pipe)
{
	if (pipe == NULL)
	{
		return;
	}

	pipe_close_read(pipe);
	pipe_close_write(pipe);

	free(pipe);
}

int pipe_read_fd(const Pipe *pipe)
{
	if (pipe == NULL)
	{
		return -1;
	}

	return pipe->read_fd;
}

int pipe_write_fd(const Pipe *pipe)
{
	if (pipe == NULL)
	{
		return -1;
	}

	return pipe->write_fd;
}

int pipe_clear_cloexec_read(Pipe *pipe)
{
	if (pipe == NULL || pipe->read_fd == PIPE_CLOSED_FD)
	{
		return -1;
	}

	return pipe_clear_fd_cloexec(pipe->read_fd);
}

int pipe_clear_cloexec_write(Pipe *pipe)
{
	if (pipe == NULL || pipe->write_fd == PIPE_CLOSED_FD)
	{
		return -1;
	}

	return pipe_clear_fd_cloexec(pipe->write_fd);
}

int pipe_close_read(Pipe *pipe)
{
	if (pipe == NULL)
	{
		return -1;
	}

	if (pipe->read_fd != PIPE_CLOSED_FD)
	{
		close(pipe->read_fd);
		pipe->read_fd = PIPE_CLOSED_FD;
	}

	return 0;
}

int pipe_close_write(Pipe *pipe)
{
	if (pipe == NULL)
	{
		return -1;
	}

	if (pipe->write_fd != PIPE_CLOSED_FD)
	{
		close(pipe->write_fd);
		pipe->write_fd = PIPE_CLOSED_FD;
	}

	return 0;
}
