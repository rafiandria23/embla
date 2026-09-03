#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "embla/log.h"
#include "embla/string.h"
#include "embla/unix_socket.h"

struct UnixListener
{
	int fd;
	char *path;
};

static int unix_socket_set_fd_cloexec(int fd)
{
	int flags = fcntl(fd, F_GETFD);

	if (flags == -1)
	{
		return -1;
	}

	return fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
}

static int unix_socket_build_address(
	struct sockaddr_un *addr,
	const char *path)
{
	if (strlen(path) >= sizeof(addr->sun_path))
	{
		embla_log_error("unix socket path too long");
		return -1;
	}

	memset(addr, 0, sizeof(*addr));
	addr->sun_family = AF_UNIX;
	strncpy(addr->sun_path, path, sizeof(addr->sun_path) - 1);

	return 0;
}

UnixListener *unix_listener_create(
	const char *path,
	int backlog)
{
	if (path == NULL)
	{
		return NULL;
	}

	struct sockaddr_un addr;

	if (unix_socket_build_address(&addr, path) != 0)
	{
		return NULL;
	}

	int fd = socket(AF_UNIX, SOCK_STREAM, 0);

	if (fd < 0)
	{
		embla_log_error("failed to create unix socket");
		return NULL;
	}

	if (unix_socket_set_fd_cloexec(fd) != 0)
	{
		embla_log_error("failed to set unix socket close-on-exec");

		close(fd);

		return NULL;
	}

	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
	{
		embla_log_error("failed to bind unix socket");

		close(fd);

		return NULL;
	}

	if (listen(fd, backlog) != 0)
	{
		embla_log_error("failed to listen to unix socket");

		close(fd);
		unlink(path);

		return NULL;
	}

	char *path_copy = embla_strdup(path);

	if (path_copy == NULL)
	{
		embla_log_error("failed to duplicate unix socket path");

		close(fd);
		unlink(path);

		return NULL;
	}

	UnixListener *listener = malloc(sizeof(*listener));

	if (listener == NULL)
	{
		embla_log_error("failed to allocate unix listener");

		close(fd);
		unlink(path);
		free(path_copy);

		return NULL;
	}

	listener->fd = fd;
	listener->path = path_copy;

	return listener;
}

void unix_listener_destroy(UnixListener *listener)
{
	if (listener == NULL)
	{
		return;
	}

	close(listener->fd);
	unlink(listener->path);
	free(listener->path);
	free(listener);
}

int unix_listener_fd(const UnixListener *listener)
{
	if (listener == NULL)
	{
		return -1;
	}

	return listener->fd;
}

int unix_listener_accept(const UnixListener *listener)
{
	if (listener == NULL)
	{
		return -1;
	}

	int fd = accept(listener->fd, NULL, NULL);

	if (fd < 0)
	{
		embla_log_error("failed to accept unix socket connection");
		return -1;
	}

	if (unix_socket_set_fd_cloexec(fd) != 0)
	{
		embla_log_error("failed to set accepted connection close-on-exec");

		close(fd);

		return -1;
	}

	return fd;
}

int unix_socket_connect(const char *path)
{
	if (path == NULL)
	{
		return -1;
	}

	struct sockaddr_un addr;

	if (unix_socket_build_address(&addr, path) != 0)
	{
		return -1;
	}

	int fd = socket(AF_UNIX, SOCK_STREAM, 0);

	if (fd < 0)
	{
		embla_log_error("failed to create unix socket");
		return -1;
	}

	if (unix_socket_set_fd_cloexec(fd) != 0)
	{
		embla_log_error("failed to set unix socket close-on-exec");

		close(fd);

		return -1;
	}

	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
	{
		embla_log_error("failed to connect to unix socket");

		close(fd);

		return -1;
	}

	return fd;
}
