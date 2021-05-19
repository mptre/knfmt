/*
 * Statement cpp branch.
 */

void
start_init(void *arg)
{
	for (pathp = &initpaths[0]; (path = *pathp) != NULL; pathp++) {
#ifdef MACHINE_STACK_GROWS_UP
		ucp = (char *)addr;
#else
		ucp = (char *)(addr + PAGE_SIZE);
#endif
		flagsp = flags;
#ifdef MACHINE_STACK_GROWS_UP
		arg0 = ucp;
#else
		arg0 = ucp + PAGE_SIZE;
#endif
	}
}
