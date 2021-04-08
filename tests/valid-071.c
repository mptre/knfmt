/*
 * lexer_peek_type() bug.
 */

int
main(void)
{
	*--wp = genv->loc->next->argv[0];
}
