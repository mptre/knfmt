/*
 * Nested cpp branch.
 */

int
main(void)
{
	while
#ifdef ONE
	    (0)
#ifdef TWO
		return 0;
#else
		return 1;
#endif /* TWO */
#else
	    (1)
		return 2;
#endif /* ONE */
}
