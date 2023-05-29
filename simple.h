struct options;

enum simple_pass {
	SIMPLE_DECL,
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

int	simple_enter(struct simple *, enum simple_pass, int, int *);
void	simple_leave(struct simple *, enum simple_pass, int);
int	is_simple_enabled(const struct simple *, enum simple_pass);
int	is_simple_any_enabled(const struct simple *);

int	simple_disable(struct simple *);
void	simple_enable(struct simple *, int);
