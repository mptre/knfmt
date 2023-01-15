#include "parser-stmt.h"

#include "doc.h"
#include "expr.h"
#include "lexer.h"
#include "parser-expr.h"
#include "parser-priv.h"
#include "style.h"
#include "token.h"

int
parser_stmt_return(struct parser *pr, struct doc *dc)
{
	struct lexer *lx = pr->pr_lx;
	struct doc *concat, *expr;
	struct token *tk;

	if (!lexer_if(lx, TOKEN_RETURN, &tk))
		return parser_none(pr);

	concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
	parser_token_trim_after(pr, tk);
	doc_token(tk, concat);
	if (!lexer_peek_if(lx, TOKEN_SEMI, NULL)) {
		int error;

		doc_literal(" ", concat);
		error = parser_expr(pr, &expr, &(struct parser_expr_arg){
		    .dc		= concat,
		    .indent	= style(pr->pr_st, ContinuationIndentWidth),
		    .flags	= EXPR_EXEC_NOPARENS,
		});
		if (error & HALT)
			return parser_fail(pr);
	} else {
		expr = concat;
	}
	if (lexer_expect(lx, TOKEN_SEMI, &tk))
		doc_token(tk, expr);
	return parser_good(pr);
}
