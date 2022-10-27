struct lexer;
struct token;

#define PARSER_TYPE_CAST	0x00000001u
#define PARSER_TYPE_ARG		0x00000002u

int	parser_type_peek(struct lexer *, struct token **, unsigned int);
int	parser_type_exec(struct lexer *, struct token **, unsigned int);
