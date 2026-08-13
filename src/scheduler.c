#include <stdlib.h>

#include "embla/log.h"
#include "embla/scheduler.h"

#define SCHEDULER_INITIAL_CAPACITY 16

struct Scheduler
{
	Process **ready_queue;

	size_t capacity;
	size_t head;
	size_t tail;
	size_t count;

	Process *current;
};

Scheduler *scheduler_create(void)
{
	Scheduler *scheduler = malloc(sizeof(*scheduler));

	if (scheduler == NULL)
	{
		embla_log_error("failed to allocate scheduler");
		return NULL;
	}

	scheduler->ready_queue = calloc(SCHEDULER_INITIAL_CAPACITY, sizeof(*scheduler->ready_queue));

	if (scheduler->ready_queue == NULL)
	{
		embla_log_error("failed to allocate ready queue");
		free(scheduler);
		return NULL;
	}

	scheduler->capacity = SCHEDULER_INITIAL_CAPACITY;
	scheduler->head = 0;
	scheduler->tail = 0;
	scheduler->count = 0;
	scheduler->current = NULL;

	return scheduler;
}

void scheduler_destroy(Scheduler *scheduler)
{
	if (scheduler == NULL)
	{
		return;
	}

	free(scheduler->ready_queue);
	free(scheduler);
}

int scheduler_add(Scheduler *scheduler, Process *process)
{
	if (scheduler == NULL || process == NULL)
	{
		return -1;
	}

	if (scheduler->count >= scheduler->capacity)
	{
		embla_log_error("scheduler ready queue is full");
		return -1;
	}

	scheduler->ready_queue[scheduler->tail] = process;

	scheduler->tail = (scheduler->tail + 1) % scheduler->capacity;

	scheduler->count++;

	return 0;
}

Process *scheduler_next(Scheduler *scheduler)
{
	if (scheduler == NULL)
	{
		return NULL;
	}

	if (scheduler->count == 0)
	{
		return NULL;
	}

	Process *process = scheduler->ready_queue[scheduler->head];

	scheduler->ready_queue[scheduler->head] = NULL;

	scheduler->head = (scheduler->head + 1) % scheduler->capacity;

	scheduler->count--;

	return process;
}

Process *scheduler_current(const Scheduler *scheduler)
{
	if (scheduler == NULL)
	{
		return NULL;
	}

	return scheduler->current;
}

int scheduler_dispatch(Scheduler *scheduler)
{
	if (scheduler == NULL)
	{
		return -1;
	}

	if (scheduler->current != NULL)
	{
		return -1;
	}

	Process *process = scheduler_next(scheduler);

	if (process == NULL)
	{
		return -1;
	}

	if (process_transition(process, PROCESS_RUNNING) != 0)
	{
		return -1;
	}

	scheduler->current = process;

	return 0;
}

int scheduler_yield(Scheduler *scheduler)
{
	if (scheduler == NULL)
	{
		return -1;
	}

	Process *current = scheduler->current;

	if (current == NULL)
	{
		return -1;
	}

	if (scheduler->count >= scheduler->capacity)
	{
		return -1;
	}

	if (process_transition(current, PROCESS_READY) != 0)
	{
		return -1;
	}

	scheduler->current = NULL;

	return scheduler_add(scheduler, current);
}

size_t scheduler_ready_count(const Scheduler *scheduler)
{
	if (scheduler == NULL)
	{
		return 0;
	}

	return scheduler->count;
}
