#include "embla/embla.h"

int main(void)
{
	Embla *embla = embla_create();

	if (embla == NULL)
	{
		return 1;
	}

	int result = embla_run(embla);

	embla_destroy(embla);

	return result != 0;
}
