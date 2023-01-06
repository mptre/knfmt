struct parser;
struct doc;

#define PARSER_ATTRIBUTES_HARDLINE		0x00000001u

int	parser_attributes(struct parser *, struct doc *, struct doc **,
    unsigned int);
