/*
 * Two cpp branches without tokens in between.
 */

int
main(void)
{
#if NIOAPIC > 0
	return 0;
#endif

#if NLAPIC > 0
	return 1;
#endif
}

#if 1
#include "foo.h"
#endif
