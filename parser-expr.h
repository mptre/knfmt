struct doc;
struct parser;
struct token;

int	parser_expr(struct parser *, struct doc *, struct doc **,
    const struct token *, unsigned int, unsigned int);
