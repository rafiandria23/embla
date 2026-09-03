#ifndef EMBLA_PIPE_H
#define EMBLA_PIPE_H

typedef struct Pipe Pipe;

Pipe *pipe_create(void);

void pipe_destroy(Pipe *pipe);

int pipe_read_fd(const Pipe *pipe);
int pipe_write_fd(const Pipe *pipe);

int pipe_clear_cloexec_read(Pipe *pipe);
int pipe_clear_cloexec_write(Pipe *pipe);

int pipe_close_read(Pipe *pipe);
int pipe_close_write(Pipe *pipe);

#endif