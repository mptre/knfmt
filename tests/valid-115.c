/*
 * Broken code hidden behind cpp.
 */

void *
malloc(void)
{
#ifdef DIAGNOSTIC
	if (1) {
	}
#endif /* DIAGNOSTIC */

#ifdef KMEMSTATS
	KMEMSTATS();
out:
	kbp->kb_calls++;
#else
out:
#endif
	mtx_leave(&malloc_mtx);
}

void
free(void)
{
#ifdef DIAGNOSTIC
#if 0
	if (freedsize == 0) {
#endif
	NO_DIAGNOSTIC();
#endif /* DIAGNOSTIC */
}
