#ifndef EMBLA_UNIX_SOCKET_H
#define EMBLA_UNIX_SOCKET_H

typedef struct UnixListener UnixListener;

UnixListener *unix_listener_create(
	const char *path,
	int backlog);

void unix_listener_destroy(UnixListener *listener);

int unix_listener_fd(const UnixListener *listener);

int unix_listener_accept(const UnixListener *listener);

int unix_socket_connect(const char *path);

typedef struct UnixDatagramSocket UnixDatagramSocket;

UnixDatagramSocket *unix_datagram_socket_create(const char *path);

void unix_datagram_socket_destroy(UnixDatagramSocket *socket);

int unix_datagram_socket_fd(const UnixDatagramSocket *socket);

int unix_datagram_socket_send(
	const char *path,
	const char *message);

#endif
