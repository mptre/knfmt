/*
 * Dangling else statement.
 */

int
main(void)
{
	if (0)
		return 0;
	else
		return 1;
	else
		return 2;
}
