/*
 * Redundant parenthesis around expression preceded with comment.
 */

int
main(void)
{
	if (/* DISABLES CODE */(1))
		return 0;
	return 1;
}
