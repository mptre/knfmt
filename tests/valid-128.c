/*
 * Broken code hidden behind cpp.
 */

void
free(void *addr, int type, size_t freedsize)
{

#ifdef DIAGNOSTIC
	if (addr < (void *)kmembase || addr >= (void *)kmemlimit)
		panic("free: non-malloced addr %p type %s", addr,
		    memname[type]);
#endif

	if (size > MAXALLOCSAVE)
		size = kup->ku_pagecnt << PAGE_SHIFT;
#ifdef DIAGNOSTIC
#if 0
	if (freedsize == 0) {
		static int zerowarnings;
		if (zerowarnings < 5) {
			zerowarnings++;
#ifdef DDB
			db_stack_dump();
#endif /* DDB */
	}
#endif /* 0 */

#endif /* DIAGNOSTIC */
}
