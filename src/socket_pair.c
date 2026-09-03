#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include "embla/log.h"
#include "embla/socket_pair.h"

#define SOCKET_PAIR_CLOSED_FD (-1)

struct SocketPair
{
	int first_fd;
	int second_fd;
};

static int socket_pair_set_fd_cloexec(int fd)
{
	int flags = fcntl(fd, F_GETFD);

	if (flags == -1)
	{
		return -1;
	}

	return fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
}

static int socket_pair_clear_fd_cloexec(int fd)
{
	int flags = fcntl(fd, F_GETFD);

	if (flags == -1)
	{
		return -1;
	}

	return fcntl(fd, F_SETFD, flags & ~FD_CLOEXEC);
}

SocketPair *socket_pair_create(int type)
{
	int fds[2];

	if (socketpair(AF_UNIX, type, 0, fds) != 0)
	{
		embla_log_error("failed to create socket pair");
		return NULL;
	}

	if (
		socket_pair_set_fd_cloexec(fds[0]) != 0 ||
		socket_pair_set_fd_cloexec(fds[1]) != 0)
	{
		embla_log_error("failed to set socket pair close-on-exec");

		close(fds[0]);
		close(fds[1]);

		return NULL;
	}

	SocketPair *pair = malloc(sizeof(*pair));

	if (pair == NULL)
	{
		embla_log_error("failed to allocate socket pair");

		close(fds[0]);
		close(fds[1]);

		return NULL;
	}

	pair->first_fd = fds[0];
	pair->second_fd = fds[1];

	return pair;
}

void socket_pair_destroy(SocketPair *pair)
{
	if (pair == NULL)
	{
		return;
	}

	socket_pair_close_first(pair);
	socket_pair_close_second(pair);

	free(pair);
}

int socket_pair_first_fd(const SocketPair *pair)
{
	if (pair == NULL)
	{
		return -1;
	}

	return pair->first_fd;
}

int socket_pair_second_fd(const SocketPair *pair)
{
	if (pair == NULL)
	{
		return -1;
	}

	return pair->second_fd;
}

int socket_pair_clear_cloexec_first(SocketPair *pair)
{
	if (pair == NULL || pair->first_fd == SOCKET_PAIR_CLOSED_FD)
	{
		return -1;
	}

	return socket_pair_clear_fd_cloexec(pair->first_fd);
}

int socket_pair_clear_cloexec_second(SocketPair *pair)
{
	if (pair == NULL || pair->second_fd == SOCKET_PAIR_CLOSED_FD)
	{
		return -1;
	}

	return socket_pair_clear_fd_cloexec(pair->second_fd);
}

int socket_pair_close_first(SocketPair *pair)
{
	if (pair == NULL)
	{
		return -1;
	}

	if (pair->first_fd != SOCKET_PAIR_CLOSED_FD)
	{
		close(pair->first_fd);
		pair->first_fd = SOCKET_PAIR_CLOSED_FD;
	}

	return 0;
}

int socket_pair_close_second(SocketPair *pair)
{
	if (pair == NULL)
	{
		return -1;
	}

	if (pair->second_fd != SOCKET_PAIR_CLOSED_FD)
	{
		close(pair->second_fd);
		pair->second_fd = SOCKET_PAIR_CLOSED_FD;
	}

	return 0;
}
