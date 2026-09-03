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

static size_t scheduler_advance(const Scheduler *scheduler, size_t index)
{
	return (index + 1) % scheduler->capacity;
}

static size_t scheduler_retreat(const Scheduler *scheduler, size_t index)
{
	return (index + scheduler->capacity - 1) % scheduler->capacity;
}

static size_t scheduler_index_at(const Scheduler *scheduler, size_t offset)
{
	return (scheduler->head + offset) % scheduler->capacity;
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

	scheduler->tail = scheduler_advance(scheduler, scheduler->tail);

	scheduler->count++;

	return 0;
}

int scheduler_remove(Scheduler *scheduler, Process *process)
{
	if (scheduler == NULL || process == NULL)
	{
		return -1;
	}

	if (scheduler->current == process)
	{
		scheduler->current = NULL;
		return 0;
	}

	for (size_t i = 0; i < scheduler->count; i++)
	{
		size_t index = scheduler_index_at(scheduler, i);

		if (scheduler->ready_queue[index] != process)
		{
			continue;
		}

		// Shift all subsequent elements one position toward the removed element
		for (size_t j = i; j + 1 < scheduler->count; j++)
		{
			size_t current_index = scheduler_index_at(scheduler, j);
			size_t next_index = scheduler_index_at(scheduler, j + 1);

			scheduler->ready_queue[current_index] = scheduler->ready_queue[next_index];
		}

		size_t last_index = scheduler_index_at(scheduler, scheduler->count - 1);

		scheduler->ready_queue[last_index] = NULL;
		scheduler->tail = scheduler_retreat(scheduler, scheduler->tail);
		scheduler->count--;

		return 0;
	}

	return -1;
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

	scheduler->head = scheduler_advance(scheduler, scheduler->head);

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
		embla_log_error("dispatched process failed to transition to RUNNING");

		scheduler_add(scheduler, process);

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
