struct doc;
struct parser;
struct ruler;
struct token;

int	parser_type(struct parser *, struct doc *, const struct token *,
    struct ruler *);
