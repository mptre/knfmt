#include "config.h"

extern int	unused;

#ifndef HAVE_PLEDGE

#include "cdefs.h"

int
pledge(const char *UNUSED(promises), const char *UNUSED(execpromises))
{
	return 0;
}

#endif
