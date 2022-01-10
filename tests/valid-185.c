/*
 * Switch case with cpp branch.
 */

int
main(void)
{
	switch (port) {
	default:
#ifdef DIAGNOSTIC
		printf("eso_set_gain: bad port %u", port);
		return;
		/* NOTREACHED */
#else
		return;
#endif
	}
}
