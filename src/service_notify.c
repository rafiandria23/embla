#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "embla/log.h"
#include "embla/service_notify.h"
#include "embla/string.h"
#include "embla/unix_socket.h"

struct ServiceNotification
{
	int ready;
	int stopping;
	char *status;
};

static void service_notification_apply_line(
	ServiceNotification *notification,
	char *line)
{
	char *equals = strchr(line, '=');

	if (equals == NULL)
	{
		return;
	}

	*equals = '\0';

	const char *key = line;
	const char *value = equals + 1;

	if (strcmp(key, "READY") == 0)
	{
		if (strcmp(value, "1") == 0)
		{
			notification->ready = 1;
		}
	}
	else if (strcmp(key, "STOPPING") == 0)
	{
		if (strcmp(value, "1") == 0)
		{
			notification->stopping = 1;
		}
	}
	else if (strcmp(key, "STATUS") == 0)
	{
		free(notification->status);
		notification->status = embla_strdup(value);
	}
}

ServiceNotification *service_notification_parse(const char *message)
{
	if (message == NULL)
	{
		return NULL;
	}

	ServiceNotification *notification = calloc(1, sizeof(*notification));

	if (notification == NULL)
	{
		embla_log_error("failed to allocate service notification");
		return NULL;
	}

	char *working_copy = embla_strdup(message);

	if (working_copy == NULL)
	{
		embla_log_error("failed to duplicate message for parsing");

		free(notification);

		return NULL;
	}

	char *saveptr = NULL;
	char *line = strtok_r(working_copy, "\n", &saveptr);

	while (line != NULL)
	{
		service_notification_apply_line(notification, line);
		line = strtok_r(NULL, "\n", &saveptr);
	}

	free(working_copy);

	return notification;
}

void service_notification_destroy(ServiceNotification *notification)
{
	if (notification == NULL)
	{
		return;
	}

	free(notification->status);
	free(notification);
}

int service_notification_is_ready(const ServiceNotification *notification)
{
	if (notification == NULL)
	{
		return 0;
	}

	return notification->ready;
}

int service_notification_is_stopping(const ServiceNotification *notification)
{
	if (notification == NULL)
	{
		return 0;
	}

	return notification->stopping;
}

const char *service_notification_get_status(
	const ServiceNotification *notification)
{
	if (notification == NULL)
	{
		return NULL;
	}

	return notification->status;
}

int service_notify_ready(const char *path)
{
	return unix_datagram_socket_send(path, "READY=1\n");
}

int service_notify_stopping(const char *path)
{
	return unix_datagram_socket_send(path, "STOPPING=1\n");
}

int service_notify_status(
	const char *path,
	const char *status)
{
	if (status == NULL || strchr(status, '\n') != NULL)
	{
		embla_log_error("service status must be non-NULL and contain no newline");
		return -1;
	}

	size_t message_length = strlen("STATUS=") + strlen(status) + 2;

	char *message = malloc(message_length);

	if (message == NULL)
	{
		embla_log_error("failed to allocate status message");
		return -1;
	}

	snprintf(message, message_length, "STATUS=%s\n", status);

	int result = unix_datagram_socket_send(path, message);

	free(message);

	return result;
}
