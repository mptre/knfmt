/*
 * Redundant parenthesis around return expression.
 */

int
test1(void)
{
	return (1);
}

int
test2(void)
{
	return (1) + 2;
}

int
test3(void)
{
	return (int)1;
}
