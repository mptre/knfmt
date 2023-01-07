#include "parser-expr.h"

#include "expr.h"
#include "parser-priv.h"
#include "parser.h"

int
parser_expr(struct parser *pr, struct doc *dc, struct doc **expr,
    const struct token *stop, struct ruler *rl, unsigned int indent,
    unsigned int flags)
{
	struct parser_private *pp = parser_get_private(pr);
	const struct expr_exec_arg ea = {
		.st		= pp->st,
		.op		= pp->op,
		.lx		= pp->lx,
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
