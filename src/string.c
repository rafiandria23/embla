#include <stdlib.h>
#include <string.h>

#include "embla/string.h"

char *embla_strdup(const char *string)
{
	if (string == NULL)
	{
		return NULL;
	}

	size_t length = strlen(string) + 1;

	char *copy = malloc(length);

	if (copy == NULL)
	{
		return NULL;
	}

	memcpy(copy, string, length);

	return copy;
}
