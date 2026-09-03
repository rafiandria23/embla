#ifndef EMBLA_SOCKET_PAIR_H
#define EMBLA_SOCKET_PAIR_H

typedef struct SocketPair SocketPair;

SocketPair *socket_pair_create(int type);

void socket_pair_destroy(SocketPair *pair);

int socket_pair_first_fd(const SocketPair *pair);
int socket_pair_second_fd(const SocketPair *pair);

int socket_pair_clear_cloexec_first(SocketPair *pair);
int socket_pair_clear_cloexec_second(SocketPair *pair);

int socket_pair_close_first(SocketPair *pair);
int socket_pair_close_second(SocketPair *pair);

#endif
