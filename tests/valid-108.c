/*
 * Brace initializers with cpp branch
 */

const struct sysctl_bounded_args	kern_vars[] = {
	{
		KERN_SYSVMSG,
#ifdef SYSVMSG
		&int_one,
#else
		    &int_zero,
#endif
		SYSCTL_INT_READONLY
	},
};
