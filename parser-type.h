struct lexer;
struct parser_context;
struct token;

struct parser_type {
	struct token	*pt_beg;
	struct token	*pt_end;
	struct token	*pt_align;
};

#define PARSER_TYPE_CAST	0x00000001u
#define PARSER_TYPE_ARG		0x00000002u

int	parser_type_peek(struct parser_context *, struct parser_type *,
    unsigned int);
int	parser_type_parse(struct parser_context *, struct parser_type *,
    unsigned int);
