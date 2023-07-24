/*
 * Redundant parenthesis around expression.
 */

int
main(void)
{
	if ((0))
		return (1);

	while (((0) != 0))
		continue;

	return (0);
}
