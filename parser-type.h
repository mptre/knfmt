struct doc;
struct parser;
struct ruler;
struct token;

#define PARSER_TYPE_CAST		0x00000001u
#define PARSER_TYPE_ARG			0x00000002u
#define PARSER_TYPE_EXPR		0x00000004u

int	parser_type_peek(struct parser *, struct token **, unsigned int);
int	parser_type(struct parser *, struct doc *, const struct token *,
    struct ruler *);
