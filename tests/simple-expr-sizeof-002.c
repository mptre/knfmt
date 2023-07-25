/*
 * Missing parenthesis around sizeof argument in nested expression.
 */

int
main(void)
{
	return (sizeof int);
	return ((sizeof int));
}
