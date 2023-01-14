#include "parser-expr.h"

#include "cdefs.h"
#include "expr.h"
#include "lexer.h"
#include "parser-priv.h"
#include "parser-type.h"
#include "parser.h"	/* parser_expr_recover */
#include "token.h"

static int	expr_recover_cast(const struct expr_exec_arg *, struct doc *,
    void *);

int
parser_expr_peek(struct parser *pr, struct token **tk)
{
	const struct expr_exec_arg ea = {
		.st		= pr->pr_st,
		.op		= pr->pr_op,
		.lx		= pr->pr_lx,
		.callbacks	= {
			.recover	= parser_expr_recover,
			.recover_cast	= expr_recover_cast,
			.arg		= pr,
		},
	};
	struct lexer_state s;
	struct lexer *lx = pr->pr_lx;
	int peek = 0;

	lexer_peek_enter(lx, &s);
	if (expr_peek(&ea)) {
		peek = 1;
		lexer_back(lx, tk);
	}
	lexer_peek_leave(lx, &s);
	return peek;
}

int
parser_expr(struct parser *pr, struct doc **expr, struct parser_expr_arg *arg)
{
	const struct expr_exec_arg ea = {
		.st		= pr->pr_st,
		.op		= pr->pr_op,
		.lx		= pr->pr_lx,
		.rl		= arg->rl,
		.dc		= arg->dc,
		.stop		= arg->stop,
		.indent		= arg->indent,
		.flags		= arg->flags,
		.callbacks	= {
			.recover	= parser_expr_recover,
			.recover_cast	= expr_recover_cast,
			.arg		= pr,
		},
	};
	struct doc *ex;

	ex = expr_exec(&ea);
	if (ex == NULL)
		return parser_none(pr);
	if (expr != NULL)
		*expr = ex;
	return parser_good(pr);
}

static int
expr_recover_cast(const struct expr_exec_arg *UNUSED(ea), struct doc *dc,
    void *arg)
{
	struct lexer_state s;
	struct parser *pr = arg;
	struct lexer *lx = pr->pr_lx;
	struct token *tk;
	int peek = 0;

	lexer_peek_enter(lx, &s);
	if (parser_type_peek(pr, &tk, PARSER_TYPE_CAST) &&
	    lexer_seek(lx, token_next(tk)) &&
	    lexer_if(lx, TOKEN_RPAREN, NULL) && !lexer_if(lx, LEXER_EOF, NULL))
		peek = 1;
	lexer_peek_leave(lx, &s);
	if (!peek)
		return 0;
	return parser_type(pr, dc, tk, NULL) & GOOD;
}
