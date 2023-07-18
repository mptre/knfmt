struct doc;
struct parser;
struct ruler;

#define PARSER_TYPE_CAST		0x00000001u
#define PARSER_TYPE_ARG			0x00000002u
#define PARSER_TYPE_EXPR		0x00000004u

struct parser_type {
	struct token    *end;
	/* Optional token to insert ruler alignment after. */
	struct token    *align;
	/* Optional token denoting start of arguments for function pointers. */
	struct token    *args;
};

int	parser_type_peek(struct parser *, struct parser_type *, unsigned int);
int	parser_type(struct parser *, struct doc *, struct parser_type *,
    struct ruler *);
