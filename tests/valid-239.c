/*
 * Never break before return expression.
 */

void
free(struct foo *p)
{
	if (p == NULL)
		return

	free(p);
}
