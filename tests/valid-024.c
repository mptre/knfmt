/*
 * Long expression with parenthesis.
 */

int
main(void)
{
	if (nest)
		if (ntokens == 0 &&
		    (lexer_if(lx, TOKEN_RPAREN, NULL) ||
		     lexer_if(lx, TOKEN_COMMA, NULL) ||
		     lexer_if(lx, TOKEN_SEMI, NULL)))
			return;
}
