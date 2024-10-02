/*
 * Statement w/o braces regression.
 */

int
main(void)
{
	if (0)
		return 1; // one
	// two
}
