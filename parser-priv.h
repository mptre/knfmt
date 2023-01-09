/*
 * Return values for parser routines. Only one of the following may be returned
 * but disjoint values are favored allowing the caller to catch multiple return
 * values.
 */
#define GOOD	0x00000001u
#define FAIL	0x00000002u
#define NONE	0x00000004u
#define BRCH	0x00000008u
#define HALT	(FAIL | NONE | BRCH)

struct parser_private {
	const struct options	*op;
	const struct style	*st;
	struct lexer		*lx;
};

struct parser_private	*parser_get_private(struct parser *);

int	parser_good(const struct parser *);
int	parser_none(const struct parser *);

#define parser_fail(a) \
	parser_fail0((a), __func__, __LINE__)
int	parser_fail0(struct parser *, const char *, int);

void	parser_token_trim_after(const struct parser *, struct token *);
void	parser_token_trim_before(const struct parser *, struct token *);

unsigned int	parser_width(struct parser *, const struct doc *);
