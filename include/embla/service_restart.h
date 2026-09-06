#ifndef EMBLA_SERVICE_RESTART_H
#define EMBLA_SERVICE_RESTART_H

#include "embla/embla.h"
#include "embla/service.h"

typedef enum
{
	SERVICE_TICK_NOTHING,
	SERVICE_TICK_WAITING,
	SERVICE_TICK_RESTARTED,
	SERVICE_TICK_REAPED_NO_RESTART,
	SERVICE_TICK_REAPED_RESTART_SCHEDULED,
	SERVICE_TICK_GAVE_UP,
	SERVICE_TICK_ERROR,
} ServiceTickResult;

ServiceTickResult service_tick(Service *service, Embla *embla);

#endif
