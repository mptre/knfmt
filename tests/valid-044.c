/*
 * Designated initializers.
 */

const uint64_t	pledge_syscalls[SYS_MAXSYSCALL] = {
	[SYS_exit]	= PLEDGE_ALWAYS,
	[SYS_kbind]	= PLEDGE_ALWAYS,
};
