/*
 * Brace initializers with cpp branch
 */

const struct sysctl_bounded_args	kern_vars[] = {
#ifdef SYSVMSG
	&int_one,
#else
	&int_zero,
#endif
};
