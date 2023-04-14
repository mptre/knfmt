struct parser;
struct parser_simple;

enum simple_state {
	SIMPLE_STATE_NOP	= -1,
	SIMPLE_STATE_DISABLE	= 0,
	SIMPLE_STATE_ENABLE	= 1,
	SIMPLE_STATE_IGNORE	= 2,
};

struct parser_simple {
	struct simple_stmt	*stmt;
	struct simple_decl	*decl;
	int			 nstmt;
	int			 ndecl;
};

struct parser_simple	*parser_simple_alloc(void);
void			 parser_simple_free(struct parser_simple *);

int	parser_simple_active(const struct parser *);
void	parser_simple_disable(struct parser *, struct parser_simple *);
void	parser_simple_enable(struct parser *, const struct parser_simple *);
