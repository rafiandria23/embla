#ifndef EMBLA_SERVICE_NOTIFY_H
#define EMBLA_SERVICE_NOTIFY_H

typedef struct ServiceNotification ServiceNotification;

ServiceNotification *service_notification_parse(const char *message);

void service_notification_destroy(ServiceNotification *notification);

int service_notification_is_ready(const ServiceNotification *notification);

int service_notification_is_stopping(const ServiceNotification *notification);

const char *service_notification_get_status(
	const ServiceNotification *notification);

int service_notify_ready(const char *path);

int service_notify_stopping(const char *path);

int service_notify_status(
	const char *path,
	const char *status);

#endif
