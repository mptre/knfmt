struct doc;
struct parser;
struct token;

int	parser_cpp_peek_x(struct parser *, struct token **);
int	parser_cpp_cdefs(struct parser *, struct doc *);
