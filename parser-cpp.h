struct doc;
struct parser;
struct ruler;
struct token;

#define PARSER_CPP_DECL_ROOT		0x00000001u

int	parser_cpp_peek_type(struct parser *);
int	parser_cpp_peek_decl(struct parser *, struct token **, unsigned int);
int	parser_cpp_peek_x(struct parser *, struct token **);
int	parser_cpp_x(struct parser *, struct doc *, struct ruler *);
int	parser_cpp_cdefs(struct parser *, struct doc *);
int	parser_cpp_decl_root(struct parser *, struct doc *);
