/*
 * AlignAfterOpenBracket: Align
 * AlignOperands: Align
 * ContinuationIndentWidth: 8
 */

int
main(void)
{
	if (foo() &&
	    bar())
		return 0;
	else if (foo() &&
		 bar())
		return 1;

	while (foo() &&
	       bar())
		continue;

	switch (foo() &&
		bar()) {
	default:
		return 0;
	}
}
