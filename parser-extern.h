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

struct parser_context {
	const char		*pc_path;
	const struct options	*pc_op;
	struct lexer		*pc_lx;
	struct error		*pc_er;
	unsigned int		 pc_error;
};

int	parser_good(const struct parser_context *);
int	parser_none(const struct parser_context *);

#define parser_fail(a) \
	parser_fail0((a), __func__, __LINE__)
int	parser_fail0(struct parser_context *, const char *, int);

void	parser_reset(struct parser_context *);
