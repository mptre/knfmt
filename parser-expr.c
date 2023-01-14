#include "parser-expr.h"

#include "expr.h"
#include "lexer.h"
#include "parser-priv.h"
#include "parser.h"

int
parser_expr_peek(struct parser *pr, struct token **tk)
{
	const struct expr_exec_arg ea = {
		.st		= pr->pr_st,
		.op		= pr->pr_op,
		.lx		= pr->pr_lx,
		.callbacks	= {
			.recover	= parser_expr_recover,
			.recover_cast	= parser_expr_recover_cast,
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
parser_expr(struct parser *pr, struct doc *dc, struct doc **expr,
    const struct token *stop, struct ruler *rl, unsigned int indent,
    unsigned int flags)
{
	const struct expr_exec_arg ea = {
		.st		= pr->pr_st,
		.op		= pr->pr_op,
		.lx		= pr->pr_lx,
		.rl		= rl,
		.dc		= dc,
		.stop		= stop,
		.indent		= indent,
		.flags		= flags,
		.callbacks	= {
			.recover	= parser_expr_recover,
			.recover_cast	= parser_expr_recover_cast,
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
