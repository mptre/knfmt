struct doc;
struct parser;
struct ruler;
struct token;

int	parser_expr_peek(struct parser *, struct token **);
int	parser_expr(struct parser *, struct doc *, struct doc **,
    const struct token *, struct ruler *, unsigned int, unsigned int);
