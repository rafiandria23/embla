#include <stdio.h>

#include "embla/embla.h"
#include "embla/log.h"

int main(void)
{
	Embla *embla = embla_create();

	if (embla == NULL)
	{
		embla_log_error("failed to create Embla");
		return 1;
	}

	int result = embla_run(embla);

	if (result != 0)
	{
		embla_log_error("Embla failed to run");

		embla_destroy(embla);

		return 1;
	}

	embla_destroy(embla);

	printf("Embla shut down successfully\n");

	return 0;
}
