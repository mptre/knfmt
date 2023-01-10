#include "parser-stmt-asm.h"

#include "doc.h"
#include "expr.h"
#include "lexer.h"
#include "parser-expr.h"
#include "parser-priv.h"
#include "style.h"
#include "token.h"

int
parser_stmt_asm(struct parser *pr, struct doc *dc)
{
	struct parser_private *pp = parser_get_private(pr);
	struct lexer_state s;
	struct lexer *lx = pp->lx;
	struct doc *concat, *opt;
	struct token *colon = NULL;
	struct token *assembly, *rparen, *tk;
	int peek = 0;
	int nops = 0;
	int error, i;

	lexer_peek_enter(lx, &s);
	if (lexer_if(lx, TOKEN_ASSEMBLY, &assembly)) {
		lexer_if(lx, TOKEN_VOLATILE, NULL);
		if (lexer_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, &rparen) &&
		    lexer_if(lx, TOKEN_SEMI, NULL))
			peek = 1;
	}
	lexer_peek_leave(lx, &s);
	if (!peek)
		return parser_none(pr);

	parser_token_trim_before(pr, rparen);

	concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
	if (lexer_expect(lx, TOKEN_ASSEMBLY, &assembly))
		doc_token(assembly, concat);
	if (lexer_if(lx, TOKEN_VOLATILE, &tk)) {
		doc_alloc(DOC_LINE, concat);
		doc_token(tk, concat);
		if (token_has_spaces(tk))
			doc_alloc(DOC_LINE, concat);
	}
	opt = doc_alloc_indent(style(pp->st, ContinuationIndentWidth),
	    doc_alloc(DOC_OPTIONAL, dc));
	if (lexer_expect(lx, TOKEN_LPAREN, &tk))
		doc_token(tk, opt);

	/* instructions */
	if (!lexer_peek_until(lx, TOKEN_COLON, &colon)) {
		/* Basic inline assembler, only instructions are required. */
		concat = opt;
	}
	error = parser_expr(pr, opt, NULL, colon, NULL, 0, 0);
	if (error & HALT)
		return parser_fail(pr);

	/* output and input operands */
	for (i = 0; i < 2; i++) {
		struct token *lsquare;

		if (!lexer_if(lx, TOKEN_COLON, &colon))
			break;

		concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, opt));
		if (i == 0 || nops > 0)
			doc_alloc(DOC_LINE, concat);
		doc_token(colon, concat);
		if (!lexer_peek_if(lx, TOKEN_COLON, NULL))
			doc_alloc(DOC_LINE, concat);

		if (lexer_if(lx, TOKEN_LSQUARE, &lsquare)) {
			struct token *rsquare, *symbolic;

			doc_token(lsquare, concat);
			if (lexer_expect(lx, TOKEN_IDENT, &symbolic))
				doc_token(symbolic, concat);
			if (lexer_expect(lx, TOKEN_RSQUARE, &rsquare))
				doc_token(rsquare, concat);
			doc_alloc(DOC_LINE, concat);
		}

		error = parser_expr(pr, concat, NULL, NULL, NULL, 0,
		    EXPR_EXEC_ASM);
		if (error & FAIL)
			return parser_fail(pr);
		nops = error & GOOD;
	}

	/* clobbers */
	if (lexer_if(lx, TOKEN_COLON, &tk)) {
		concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, opt));
		doc_token(tk, concat);
		if (!lexer_peek_if(lx, TOKEN_RPAREN, NULL))
			doc_alloc(DOC_LINE, concat);
		error = parser_expr(pr, concat, NULL, rparen, NULL, 0, 0);
		if (error & FAIL)
			return parser_fail(pr);
	}

	if (lexer_expect(lx, TOKEN_RPAREN, &rparen))
		doc_token(rparen, concat);
	if (lexer_expect(lx, TOKEN_SEMI, &tk))
		doc_token(tk, concat);

	return parser_good(pr);
}
