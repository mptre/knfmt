struct options;

/* Force enable pass even if simple mode is not enabled. */
#define SIMPLE_FORCE			0x00000001u
/* Ignore pass including nested invocations. */
#define SIMPLE_IGNORE			0x00000002u

enum simple_pass {
	SIMPLE_BRACES,
	SIMPLE_CPP_SORT_INCLUDES,
	SIMPLE_DECL,
	SIMPLE_DECL_PROTO,
	SIMPLE_EXPR_NOPARENS,
	SIMPLE_STATIC,
	SIMPLE_STMT,
	SIMPLE_STMT_SEMI,

	SIMPLE_LAST, /* sentinel */
};

enum simple_state {
	SIMPLE_STATE_NOP	= -1,
	SIMPLE_STATE_DISABLE	= 0,
	SIMPLE_STATE_ENABLE	= 1,
	SIMPLE_STATE_IGNORE	= 2,
};

struct simple	*simple_alloc(const struct options *);
void		 simple_free(struct simple *);

int	simple_enter(struct simple *, enum simple_pass, unsigned int, int *);
void	simple_leave(struct simple *, enum simple_pass, int);
int	is_simple_enabled(const struct simple *, enum simple_pass);

int	simple_disable(struct simple *);
void	simple_enable(struct simple *, int);
