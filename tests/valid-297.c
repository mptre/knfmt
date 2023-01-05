/*
 * Never break before closing parenthesis in call expression.
 */

int
main(void)
{
	foo(
	    bar /* comment */
	);
}
