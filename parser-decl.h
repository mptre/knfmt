struct doc;
struct parser;

/* Honor grouped declarations. */
#define PARSER_DECL_BREAK			0x00000001u
/* Emit hard line after declaration(s). */
#define PARSER_DECL_LINE			0x00000002u
/* Parsing of declarations on root level. */
#define PARSER_DECL_ROOT			0x00000004u

int	parser_decl_peek(struct parser *);
int	parser_decl(struct parser *, struct doc *, unsigned int);
