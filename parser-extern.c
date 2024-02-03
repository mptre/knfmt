#include "parser-extern.h"

#include "config.h"

#include "doc.h"
#include "lexer.h"
#include "parser-priv.h"
#include "token.h"

int
parser_extern(struct parser *pr, struct doc *dc)
{
	struct lexer_state s;
	struct lexer *lx = pr->pr_lx;
	struct token *tk;
	int peek = 0;

	lexer_peek_enter(lx, &s);
	if (lexer_if(lx, TOKEN_EXTERN, NULL) &&
	    lexer_if(lx, TOKEN_STRING, NULL) &&
	    lexer_if_pair(lx, TOKEN_LBRACE, TOKEN_RBRACE, NULL))
		peek = 1;
	lexer_peek_leave(lx, &s);
	if (!peek)
		return parser_none(pr);

	if (lexer_expect(lx, TOKEN_EXTERN, &tk))
		parser_doc_token(pr, tk, dc);
	doc_literal(" ", dc);
	if (lexer_expect(lx, TOKEN_STRING, &tk))
		parser_doc_token(pr, tk, dc);
	doc_literal(" ", dc);
	if (lexer_expect(lx, TOKEN_LBRACE, &tk))
		parser_doc_token(pr, tk, dc);
	doc_alloc(DOC_HARDLINE, dc);
	for (;;) {
		int error;

		error = parser_exec1(pr, dc);
		if (error & NONE)
			break;
		if (error & HALT)
			return error;
	}
	if (lexer_expect(lx, TOKEN_RBRACE, &tk))
		parser_doc_token(pr, tk, dc);
	if (lexer_if(lx, TOKEN_SEMI, &tk))
		parser_doc_token(pr, tk, dc);
	doc_alloc(DOC_HARDLINE, dc);
	return parser_good(pr);
}
