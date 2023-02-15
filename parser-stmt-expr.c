#include "parser-stmt-expr.h"

#include "config.h"

#include "doc.h"
#include "lexer.h"
#include "parser-expr.h"
#include "parser-priv.h"
#include "parser-stmt.h"
#include "parser-type.h"
#include "style.h"
#include "token.h"

int
parser_stmt_expr(struct parser *pr, struct doc *dc)
{
	struct lexer_state s;
	struct lexer *lx = pr->pr_lx;
	struct doc *expr = NULL;
	struct token *ident, *lparen, *nx, *rparen, *semi;
	int peek = 1;
	int error;

	if (parser_type_peek(pr, NULL, 0))
		return parser_none(pr);
	if (!lexer_peek_until_freestanding(lx, TOKEN_SEMI, NULL, &semi))
		return parser_none(pr);
	if (!parser_expr_peek(pr, &nx) || token_next(nx) != semi)
		return parser_none(pr);

	/*
	 * Do not confuse a loop construct hidden behind cpp followed by a sole
	 * statement:
	 *
	 * 	foreach()
	 * 		func();
	 */
	lexer_peek_enter(lx, &s);
	if (lexer_if(lx, TOKEN_IDENT, &ident) &&
	    lexer_peek_if(lx, TOKEN_LPAREN, &lparen) &&
	    lexer_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, &rparen) &&
	    token_cmp(lparen, rparen) == 0 &&
	    lexer_pop(lx, &nx) && nx != semi &&
	    token_cmp(ident, nx) < 0 && token_cmp(nx, semi) <= 0)
		peek = 0;
	lexer_peek_leave(lx, &s);
	if (!peek)
		return parser_none(pr);

	error = parser_expr(pr, &expr, &(struct parser_expr_arg){
	    .dc		= dc,
	    .indent	= style(pr->pr_st, ContinuationIndentWidth),
	});
	if (error & HALT)
		return parser_fail(pr);
	if (lexer_expect(lx, TOKEN_SEMI, &semi))
		doc_token(semi, expr);
	return parser_good(pr);
}

int
parser_stmt_expr_gnu(struct parser *pr, struct doc *dc)
{
	return parser_stmt_block(pr, &(struct parser_stmt_block_arg){
	    .head	= dc,
	    .tail	= dc,
	    .flags	= PARSER_STMT_BLOCK_EXPR_GNU,
	});
}
