struct lexer;
struct token;

struct parser_type {
	struct token	*pt_beg;
	struct token	*pt_end;
};

#define PARSER_TYPE_CAST	0x00000001u
#define PARSER_TYPE_ARG		0x00000002u

int	parser_type_peek(struct lexer *, struct parser_type *, unsigned int);
int	parser_type_exec(struct lexer *, struct parser_type *, unsigned int);
