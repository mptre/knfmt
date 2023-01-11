#include "parser-cpp.h"

#include "lexer.h"
#include "parser-priv.h"
#include "token.h"

/*
 * Returns non-zero if the next tokens denotes a X macro. That is, something
 * that looks like a function call but is not followed by a semicolon nor comma
 * if being part of an initializer. One example are the macros provided by
 * RBT_PROTOTYPE(9).
 */
int
parser_cpp_peek_x(struct parser *pr, struct token **tk)
{
	struct lexer_state s;
	struct lexer *lx = pr->pr_lx;
	struct token *pv = NULL;
	struct token *ident, *rparen;
	int peek = 0;

	(void)lexer_back(lx, &pv);
	lexer_peek_enter(lx, &s);
	while (lexer_if_flags(lx, TOKEN_FLAG_STORAGE, NULL))
		continue;
	if (lexer_if(lx, TOKEN_IDENT, &ident) &&
	    lexer_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, &rparen)) {
		const struct token *nx;

		/*
		 * The previous token must not reside on the same line as the
		 * identifier. The next token must reside on the next line and
		 * have the same or less indentation. This is of importance in
		 * order to not confuse loop constructs hidden behind cpp.
		 */
		nx = token_next(rparen);
		if ((pv == NULL || token_cmp(pv, ident) < 0) &&
		    (nx == NULL || (token_cmp(nx, rparen) > 0 &&
		     nx->tk_cno <= ident->tk_cno)))
			peek = 1;
	}
	lexer_peek_leave(lx, &s);
	if (peek && tk != NULL)
		*tk = rparen;
	return peek;
}
