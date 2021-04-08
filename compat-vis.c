#include "config.h"

extern int	unused;

#ifndef HAVE_VIS

#include <err.h>
#include <string.h>

int
stravis(char **outp, const char *str, int flag __attribute__((__unused__)))
{
	size_t len;

	len = strlen(str);
	*outp = strndup(str, len);
	if (*outp == NULL)
		return -1;
	return len;
}

#endif
