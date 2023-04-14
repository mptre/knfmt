struct parser;
struct parser_simple;

enum simple_pass {
	SIMPLE_DECL,
	SIMPLE_STMT,

	SIMPLE_LAST, /* sentinel */
};

enum simple_state {
	SIMPLE_STATE_NOP	= -1,
	SIMPLE_STATE_DISABLE	= 0,
	SIMPLE_STATE_ENABLE	= 1,
	SIMPLE_STATE_IGNORE	= 2,
};

struct parser_simple {
	struct simple_stmt	*stmt;
	struct simple_decl	*decl;
	int			 states[SIMPLE_LAST];
};

struct parser_simple	*parser_simple_alloc(void);
void			 parser_simple_free(struct parser_simple *);

int	parser_simple_enter(struct parser *, unsigned int, int, int *);
void	parser_simple_leave(struct parser *, unsigned int, int);
int	is_simple_enabled(const struct parser *, unsigned int);
int	is_simple_any_enabled(const struct parser *);
void	parser_simple_disable(struct parser *, struct parser_simple *);
void	parser_simple_enable(struct parser *, const struct parser_simple *);
