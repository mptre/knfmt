struct doc;
struct parser;
struct token;

enum parser_func_peek {
	PARSER_FUNC_PEEK_NONE,
	PARSER_FUNC_PEEK_DECL,
	PARSER_FUNC_PEEK_IMPL,
};

enum parser_func_peek	parser_func_peek(struct parser *, struct token **);
int			parser_func_arg(struct parser *, struct doc *,
    struct doc **, const struct token *);
