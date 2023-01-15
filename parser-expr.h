struct doc;
struct parser;
struct ruler;
struct token;

struct parser_expr_arg {
	struct doc		*dc;
	struct ruler		*rl;
	const struct token	*stop;
	unsigned int		 indent;
	unsigned int		 flags;
};

int	parser_expr_peek(struct parser *, struct token **);

int	parser_expr(struct parser *, struct doc **, struct parser_expr_arg *);
