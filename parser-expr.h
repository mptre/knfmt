#include "expr.h"

struct parser;

struct parser_expr_arg {
	struct doc		*dc;
	struct ruler		*rl;
	const struct token	*stop;
	unsigned int		 indent;
	unsigned int		 flags;     /* expr_exec() flags */
};

int	parser_expr_peek(struct parser *, struct token **);
int	parser_expr(struct parser *, struct doc **, struct parser_expr_arg *);
