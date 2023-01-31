struct doc;
struct parser;

#define PARSER_DECL_BRACES_ENUM			0x00000001u
#define PARSER_DECL_BRACES_TRIM			0x00000002u
/* Remove the given indent before emitting the right brace. */
#define PARSER_DECL_BRACES_DEDENT		0x00000004u
/* Perform conditional indentation. */
#define PARSER_DECL_BRACES_INDENT_MAYBE		0x00000008u

int	parser_braces(struct parser *, struct doc *, unsigned int,
    unsigned int);
