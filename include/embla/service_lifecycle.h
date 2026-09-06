#ifndef EMBLA_SERVICE_LIFECYCLE_H
#define EMBLA_SERVICE_LIFECYCLE_H

#include "embla/embla.h"
#include "embla/service.h"

int service_start(Service *service, Embla *embla);

int service_stop(Service *service, Embla *embla);

int service_reap(Service *service, Embla *embla);

#endif
