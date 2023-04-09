/*
 * Expression parenthesis alignment regression.
 */

int
main(void)
{
	if (a &&
	    !(b &&
	    c)) {
		return 0;
	}
}
