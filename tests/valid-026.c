/*
 * Long expression.
 */

int
main(void)
{
	if (nest) {
		if (nest) {
			if (lexer_peek_if_pair(pr->pr_lx, TOKEN_LPAREN,
			    TOKEN_RPAREN, &rparen))
				break;
		}
	}
}
