#include "parser-stmt-expr.h"

#include "config.h"

#include "lexer.h"
#include "parser-expr.h"
#include "parser-priv.h"
#include "parser-stmt.h"
#include "parser-type.h"
#include "style.h"
#include "token.h"

static int
is_loop_stmt(struct parser *pr, const struct token *semi)
{
	struct lexer_state s;
	struct lexer *lx = pr->pr_lx;
	struct token *ident, *lparen, *nx, *rparen;
	int peek = 0;

	lexer_peek_enter(lx, &s);
	if (lexer_if(lx, TOKEN_IDENT, &ident) &&
	    lexer_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, &lparen, &rparen) &&
	    lexer_peek(lx, &nx) && nx != semi && token_cmp(rparen, nx) < 0 &&
	    parser_stmt_peek(pr))
		peek = 1;
	lexer_peek_leave(lx, &s);

	return peek;
}

int
parser_stmt_expr(struct parser *pr, struct doc *dc)
{
	struct lexer *lx = pr->pr_lx;
	struct doc *expr = NULL;
	struct token *nx, *semi;
	int error;

	if (parser_type_peek(pr, NULL, 0) || !parser_expr_peek(pr, &nx))
		return parser_none(pr);
	nx = token_next(nx);
	if (nx->tk_type != TOKEN_SEMI)
		return parser_none(pr);
	semi = nx;

	/*
	 * Do not confuse a loop construct hidden behind cpp followed by a sole
	 * statement:
	 *
	 * 	foreach()
	 * 		func();
	 */
	if (is_loop_stmt(pr, semi))
		return parser_none(pr);

	error = parser_expr(pr, &expr, &(struct parser_expr_arg){
	    .dc		= dc,
	    .indent	= style(pr->pr_st, ContinuationIndentWidth),
	});
	if (error & HALT)
		return parser_fail(pr);
	if (lexer_expect(lx, TOKEN_SEMI, &semi)) {
		parser_doc_token(pr, semi, expr);
		while (lexer_peek_if(lx, TOKEN_SEMI, &nx) &&
		    token_cmp(semi, nx) == 0 &&
		    lexer_pop(lx, &nx))
			lexer_remove(lx, nx);
	}
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
