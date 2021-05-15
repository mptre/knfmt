/*
 * Declaration cpp branch.
 */

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_extern.h>
extern int	db_console;
#endif

int	x;

#ifdef CPURESET_DELAY
int	cpureset_delay = CPURESET_DELAY;
#else
int	cpureset_delay = 0;
#endif

int	x;
