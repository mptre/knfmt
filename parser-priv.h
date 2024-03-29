struct doc;
struct token;

/*
 * Return values for parser routines. Only one of the following may be returned
 * but disjoint values are favored allowing the caller to catch multiple return
 * values.
 */
#define GOOD	0x00000001
#define FAIL	0x00000002
#define NONE	0x00000004
#define BRCH	0x00000008
#define HALT	(FAIL | NONE | BRCH)

struct parser {
	const struct options	*pr_op;
	const struct style	*pr_st;
	struct simple		*pr_si;
	struct lexer		*pr_lx;
	struct buffer		*pr_scratch;
	unsigned int		 pr_error;
	unsigned int		 pr_nindent;	/* # indented stmt blocks */

	struct {
		struct simple_stmt		*stmt;
		struct simple_decl		*decl;
		struct simple_decl_proto	*decl_proto;
	} pr_simple;

	struct {
		int		 valid;
		struct token	*lbrace;
	} pr_braces;

	struct {
		struct ruler	*ruler;		/* align X macros */
	} pr_cpp;
};

int	parser_good(const struct parser *);
int	parser_none(const struct parser *);

#define parser_fail(a) \
	parser_fail0((a), __func__, __LINE__)
int	parser_fail0(struct parser *, const char *, int);

void	parser_reset(struct parser *);

void	parser_token_trim_after(const struct parser *, struct token *);
void	parser_token_trim_before(const struct parser *, struct token *);

unsigned int	parser_width(struct parser *, const struct doc *);

int	parser_exec1(struct parser *, struct doc *);
