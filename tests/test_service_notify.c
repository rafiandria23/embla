#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "embla/service_notify.h"
#include "embla/unix_socket.h"

#define CHECK(cond, msg)                                                    \
	do                                                                      \
	{                                                                       \
		if (!(cond))                                                        \
		{                                                                   \
			fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
			return -1;                                                      \
		}                                                                   \
	} while (0)

static const char *test_socket_path(void)
{
	static char path[256];

	snprintf(path, sizeof(path), "/tmp/embla_test_notify_unit_%d.sock", getpid());

	return path;
}

static int test_parse_ready(void)
{
	ServiceNotification *n = service_notification_parse("READY=1\n");

	CHECK(n != NULL, "parse should succeed");
	CHECK(
		service_notification_is_ready(n) == 1,
		"READY=1 should be seen");
	CHECK(
		service_notification_is_stopping(n) == 0,
		"STOPPING should be unset");
	CHECK(
		service_notification_get_status(n) == NULL,
		"STATUS should be unset");

	service_notification_destroy(n);

	return 0;
}

static int test_parse_stopping(void)
{
	ServiceNotification *n = service_notification_parse("STOPPING=1\n");

	CHECK(n != NULL, "parse should succeed");
	CHECK(
		service_notification_is_stopping(n) == 1,
		"STOPPING=1 should be seen");
	CHECK(
		service_notification_is_ready(n) == 0,
		"READY should be unset");

	service_notification_destroy(n);

	return 0;
}

static int test_parse_status(void)
{
	ServiceNotification *n = service_notification_parse("STATUS=starting up\n");

	CHECK(n != NULL, "parse should succeed");
	CHECK(
		strcmp(service_notification_get_status(n), "starting up") == 0,
		"STATUS text should match exactly");

	service_notification_destroy(n);

	return 0;
}

static int test_parse_multiple_lines_in_one_message(void)
{
	ServiceNotification *n = service_notification_parse("READY=1\nSTATUS=all systems go\n");

	CHECK(n != NULL, "parse should succeed");
	CHECK(
		service_notification_is_ready(n) == 1,
		"READY should be seen");
	CHECK(
		strcmp(service_notification_get_status(n), "all systems go") == 0,
		"STATUS should also be seen in the same message");

	service_notification_destroy(n);

	return 0;
}

static int test_parse_ignores_unrecognized_keys(void)
{
	ServiceNotification *n = service_notification_parse("FUTURE_KEY=something\nREADY=1\n");

	CHECK(n != NULL, "parse should succeed even with an unknown key");
	CHECK(
		service_notification_is_ready(n) == 1,
		"a recognized key elsewhere in the message should still be "
		"picked up");

	service_notification_destroy(n);

	return 0;
}

static int test_parse_ignores_malformed_lines(void)
{
	ServiceNotification *n = service_notification_parse("this line has no equals sign\nREADY=1\n");

	CHECK(n != NULL, "parse should succeed rather than fail outright");
	CHECK(
		service_notification_is_ready(n) == 1,
		"a valid line elsewhere should still be picked up");

	service_notification_destroy(n);

	return 0;
}

static int test_parse_empty_message_is_not_an_error(void)
{
	ServiceNotification *n = service_notification_parse("");

	CHECK(n != NULL, "an empty message should parse, not fail");
	CHECK(
		service_notification_is_ready(n) == 0,
		"nothing should be set");
	CHECK(
		service_notification_is_stopping(n) == 0,
		"nothing should be set");
	CHECK(
		service_notification_get_status(n) == NULL,
		"nothing should be set");

	service_notification_destroy(n);

	return 0;
}

static int test_parse_rejects_wrong_value(void)
{
	ServiceNotification *n = service_notification_parse("READY=0\n");

	CHECK(n != NULL, "parse should succeed");
	CHECK(
		service_notification_is_ready(n) == 0,
		"READY=0 must NOT be treated as ready -- only exactly "
		"READY=1 counts");

	service_notification_destroy(n);

	return 0;
}

static int test_parse_last_status_wins(void)
{
	ServiceNotification *n = service_notification_parse("STATUS=first\nSTATUS=second\n");

	CHECK(n != NULL, "parse should succeed");
	CHECK(
		strcmp(service_notification_get_status(n), "second") == 0,
		"the last STATUS= line in a message should win");

	service_notification_destroy(n);

	return 0;
}

static int test_parse_null_message_returns_null(void)
{
	CHECK(
		service_notification_parse(NULL) == NULL,
		"parsing NULL should return NULL, not crash");

	return 0;
}

static int test_destroy_null_is_safe(void)
{
	service_notification_destroy(NULL);

	return 0;
}

static int test_query_functions_null_safety(void)
{
	CHECK(
		service_notification_is_ready(NULL) == 0,
		"is_ready(NULL) should be 0");
	CHECK(
		service_notification_is_stopping(NULL) == 0,
		"is_stopping(NULL) should be 0");
	CHECK(
		service_notification_get_status(NULL) == NULL,
		"get_status(NULL) should be NULL");

	return 0;
}

static int test_notify_status_rejects_embedded_newline(void)
{
	CHECK(
		service_notify_status("/tmp/does-not-matter.sock", "bad\nstatus") == -1,
		"a status containing a newline must be rejected outright, "
		"not silently sent (it would corrupt the message framing "
		"for any parser)");

	return 0;
}

static int test_notify_ready_sends_exact_wire_format(void)
{
	const char *path = test_socket_path();

	unlink(path);

	UnixDatagramSocket *socket = unix_datagram_socket_create(path);

	CHECK(socket != NULL, "unix_datagram_socket_create should succeed");
	CHECK(
		service_notify_ready(path) == 0,
		"service_notify_ready should succeed");

	char buf[64];
	ssize_t n = read(unix_datagram_socket_fd(socket), buf, sizeof(buf) - 1);

	CHECK(n > 0, "reading the sent message should succeed");

	buf[n] = '\0';

	CHECK(
		strcmp(buf, "READY=1\n") == 0,
		"service_notify_ready must send exactly \"READY=1\\n\", "
		"byte for byte");

	unix_datagram_socket_destroy(socket);

	return 0;
}

static int test_notify_stopping_sends_exact_wire_format(void)
{
	const char *path = test_socket_path();

	unlink(path);

	UnixDatagramSocket *socket = unix_datagram_socket_create(path);

	CHECK(socket != NULL, "unix_datagram_socket_create should succeed");
	CHECK(
		service_notify_stopping(path) == 0,
		"service_notify_stopping should succeed");

	char buf[64];
	ssize_t n = read(unix_datagram_socket_fd(socket), buf, sizeof(buf) - 1);

	CHECK(n > 0, "reading the sent message should succeed");

	buf[n] = '\0';

	CHECK(
		strcmp(buf, "STOPPING=1\n") == 0,
		"service_notify_stopping must send exactly \"STOPPING=1\\n\", "
		"byte for byte");

	unix_datagram_socket_destroy(socket);

	return 0;
}

static int test_notify_status_sends_exact_wire_format(void)
{
	const char *path = test_socket_path();

	unlink(path);

	UnixDatagramSocket *socket = unix_datagram_socket_create(path);

	CHECK(socket != NULL, "unix_datagram_socket_create should succeed");
	CHECK(
		service_notify_status(path, "warming up caches") == 0,
		"service_notify_status should succeed");

	char buf[64];
	ssize_t n = read(unix_datagram_socket_fd(socket), buf, sizeof(buf) - 1);

	CHECK(n > 0, "reading the sent message should succeed");

	buf[n] = '\0';

	CHECK(
		strcmp(buf, "STATUS=warming up caches\n") == 0,
		"service_notify_status must send exactly "
		"\"STATUS=<text>\\n\", byte for byte");

	unix_datagram_socket_destroy(socket);

	return 0;
}

int main(void)
{
	struct
	{
		const char *name;
		int (*fn)(void);
	} tests[] = {
		{"parse_ready", test_parse_ready},
		{"parse_stopping", test_parse_stopping},
		{"parse_status", test_parse_status},
		{
			"parse_multiple_lines_in_one_message",
			test_parse_multiple_lines_in_one_message,
		},
		{
			"parse_ignores_unrecognized_keys",
			test_parse_ignores_unrecognized_keys,
		},
		{
			"parse_ignores_malformed_lines",
			test_parse_ignores_malformed_lines,
		},
		{
			"parse_empty_message_is_not_an_error",
			test_parse_empty_message_is_not_an_error,
		},
		{"parse_rejects_wrong_value", test_parse_rejects_wrong_value},
		{"parse_last_status_wins", test_parse_last_status_wins},
		{
			"parse_null_message_returns_null",
			test_parse_null_message_returns_null,
		},
		{"destroy_null_is_safe", test_destroy_null_is_safe},
		{
			"query_functions_null_safety",
			test_query_functions_null_safety,
		},
		{
			"notify_status_rejects_embedded_newline",
			test_notify_status_rejects_embedded_newline,
		},
		{
			"notify_ready_sends_exact_wire_format",
			test_notify_ready_sends_exact_wire_format,
		},
		{
			"notify_stopping_sends_exact_wire_format",
			test_notify_stopping_sends_exact_wire_format,
		},
		{
			"notify_status_sends_exact_wire_format",
			test_notify_status_sends_exact_wire_format,
		},
	};

	size_t count = sizeof(tests) / sizeof(tests[0]);
	int failures = 0;

	for (size_t i = 0; i < count; i++)
	{
		printf("-- %s\n", tests[i].name);

		if (tests[i].fn() != 0)
		{
			failures++;
		}
	}

	if (failures > 0)
	{
		fprintf(stderr, "%d test(s) failed\n", failures);
		return EXIT_FAILURE;
	}

	printf("all %zu unit tests passed\n", count);

	return EXIT_SUCCESS;
}
