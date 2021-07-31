/*
 * Function pointer type regression in lexer_peek_if_type().
 */

int
main(void)
{
	free(*buf);
}
