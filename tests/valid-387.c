/*
 * Branch in if/else if chain.
 */

int
main(void)
{
	if (0) {
		return;
#if 0
	} else if (0) {
		return;
#else
	} else if (0) {
		return;
#endif
	}
}
