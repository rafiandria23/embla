#include "embla/service_lifecycle.h"
#include "embla/service_restart.h"

static double service_restart_compute_delay(const Service *service)
{
	double base = service_get_restart_base_delay(service);
	double max = service_get_restart_max_delay(service);
	int count = service_get_consecutive_restart_count(service);

	int exponent = count - 1;

	if (exponent < 0)
	{
		exponent = 0;
	}

	if (exponent > 30)
	{
		exponent = 30;
	}

	double multiplier = (double)(1L << exponent);
	double delay = base * multiplier;

	if (delay > max)
	{
		delay = max;
	}

	return delay;
}

ServiceTickResult service_tick(Service *service, Embla *embla)
{
	if (service == NULL || embla == NULL)
	{
		return SERVICE_TICK_ERROR;
	}

	if (service_is_restart_pending(service))
	{
		double now = service_monotonic_now();

		if (now < service_get_restart_allowed_at(service))
		{
			return SERVICE_TICK_WAITING;
		}

		if (service_set_restart_pending(service, 0) != 0)
		{
			return SERVICE_TICK_ERROR;
		}

		if (service_start(service, embla) != 0)
		{
			return SERVICE_TICK_ERROR;
		}

		return SERVICE_TICK_RESTARTED;
	}

	int reap_result = service_reap(service, embla);

	if (reap_result < 0)
	{
		return SERVICE_TICK_ERROR;
	}

	if (reap_result == 0)
	{
		return SERVICE_TICK_NOTHING;
	}

	if (service_get_stop_requested(service))
	{
		if (service_set_stop_requested(service, 0) != 0)
		{
			return SERVICE_TICK_ERROR;
		}

		return SERVICE_TICK_REAPED_NO_RESTART;
	}

	int exit_code = service_get_last_exit_code(service);
	int term_signal = service_get_last_term_signal(service);
	int was_clean_exit = (term_signal == -1 && exit_code == 0);

	RestartPolicy policy = service_get_restart_policy(service);

	int should_restart;

	switch (policy)
	{
	case RESTART_ALWAYS:
		should_restart = 1;
		break;

	case RESTART_ON_FAILURE:
		should_restart = !was_clean_exit;
		break;

	case RESTART_NEVER:
	default:
		should_restart = 0;
		break;
	}

	if (!should_restart)
	{
		return SERVICE_TICK_REAPED_NO_RESTART;
	}

	double now = service_monotonic_now();
	double last_start = service_get_last_start_time(service);
	double stability_threshold = service_get_restart_stability_threshold(service);

	if ((now - last_start) >= stability_threshold)
	{
		if (service_set_consecutive_restart_count(service, 0) != 0)
		{
			return SERVICE_TICK_ERROR;
		}
	}

	int new_count = service_get_consecutive_restart_count(service) + 1;

	if (service_set_consecutive_restart_count(service, new_count) != 0)
	{
		return SERVICE_TICK_ERROR;
	}

	if (new_count > service_get_max_consecutive_restarts(service))
	{
		if (service_set_permanently_failed(service, 1) != 0)
		{
			return SERVICE_TICK_ERROR;
		}

		return SERVICE_TICK_GAVE_UP;
	}

	double delay = service_restart_compute_delay(service);

	if (service_set_restart_allowed_at(service, now + delay) != 0)
	{
		return SERVICE_TICK_ERROR;
	}

	if (service_set_restart_pending(service, 1) != 0)
	{
		return SERVICE_TICK_ERROR;
	}

	return SERVICE_TICK_REAPED_RESTART_SCHEDULED;
}
