/*
 * AlignAfterOpenBracket: Align
 * BreakBeforeBinaryOperators: NonAssignment
 */

int
main(void)
{
	if (0) {
		if (/* comment */
		    aaaaaa == (WWWWWWWWWWWW | RRRRRRRRRRRRR) ||
		    /* comment */
		    aaaaaa == (WWWWWWWWWWWW | XXXXXXXXXXXX | RRRRRRRRRRRRR))
			return 1;
	}

	if (1) {
		if (
		    1)
			return 2;
	}
}
