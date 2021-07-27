/*
 * Goto label indentation regression.
 */

int
main(void)
{
	goto out;

	/* comment */
out:
	return 0;
}
