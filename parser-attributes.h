struct doc;
struct parser;
struct token;

/* Emit appropriate line before the first attribute. */
#define PARSER_ATTRIBUTES_LINE		0x00000001u
/*
 * Detect attribute(s) hidden behind cpp macros after the return type as part of
 * either a function declaration or implementation.
 */
#define PARSER_ATTRIBUTES_FUNC		0x00000002u

int	parser_attributes_peek(struct parser *, struct token **, unsigned int);
int	parser_attributes(struct parser *, struct doc *, struct doc **,
    unsigned int);
