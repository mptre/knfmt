/*
 * Broken code hidden behind cpp.
 */

static
#ifndef SMALL_KERNEL
__inline__
#endif
long
BUCKETINDX(size_t sz)
{
}

/*
 * Allocate a block of memory
 */
void *
malloc(size_t size, int type, int flags)
{
#ifdef KMEMSTATS
	kup->ku_freecnt--;
out:
	ksp->ks_inuse++;
#else
out:
#endif
}

/*
 * Free a block of memory allocated by malloc.
 */
void
free(void *addr, int type, size_t freedsize)
{
#if 0
	if (freedsize == 0) {
		static int zerowarnings;
		if (zerowarnings < 5) {
			zerowarnings++;
			printf("free with zero size: (%d)\n", type);
	}
#endif
}
