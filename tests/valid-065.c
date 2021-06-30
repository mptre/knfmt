/*
 * Long expression.
 */

int
main(void)
{
	if (nest) {
		if (nest) {
			if (!lexer_peek_until_loose(lx, TOKEN_COMMA, rbrace,
			    &tk))
				break;
		}
	}
}
