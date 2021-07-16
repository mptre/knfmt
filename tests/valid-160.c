/*
 * Long expression with parenthesis alignment.
 */

int
main(void)
{
	if (a &&
	    ((b &&
	      c)))
		return 0;
}
