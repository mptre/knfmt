/*
 * Missing curly braces around single statement spanning multiple lines.
 */

int
main(void)
{
	if (0) {
	} else /* comment */
		return 0; /* comment */
}
