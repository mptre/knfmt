/*
 * Expression cpp branch.
 */

int
main(void)
{
	if (
#if 1
	    0
#elif 2
	    1
#else
	    2
#endif
	    ) {
	}
	return 0;
}
