struct parser;
struct doc;

/* Emit appropriate line before the first attribute. */
#define PARSER_ATTRIBUTES_LINE		0x00000001u

int	parser_attributes(struct parser *, struct doc *, struct doc **,
    unsigned int);
