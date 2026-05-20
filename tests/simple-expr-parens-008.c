/*
 * Parenthesis around GNU statement expressions are never redundant.
 */

int
main(void)
{
	return foo(({ 0; }));
}
