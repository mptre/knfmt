/*
 * Trailing comment after declaration block.
 */

struct rwlock		dt_lock = RWLOCK_INITIALIZER("dtlk");
volatile uint32_t	dt_tracing = 0;	/* [K] # of processes tracing */

int allowdt;
