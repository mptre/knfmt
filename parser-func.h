struct doc;
struct parser;
struct token;

int	parser_func_arg(struct parser *, struct doc *, struct doc **,
    const struct token *);
