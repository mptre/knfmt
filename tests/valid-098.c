/*
 * Function call expression through indirection.
 */

int
main(void)
{
	(*linesw[tp->t_line].l_start)(tp);
}
