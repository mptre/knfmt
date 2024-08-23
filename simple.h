struct arena_scope;
struct options;

/* Force enable pass even if simple mode is not enabled. */
#define SIMPLE_FORCE			0x00000001u
/* Ignore pass including nested invocations. */
#define SIMPLE_IGNORE			0x00000002u

enum simple_pass {
	SIMPLE_BRACES,
	SIMPLE_CPP_SORT_INCLUDES,
	SIMPLE_DECL,
	SIMPLE_DECL_FORWARD,
	SIMPLE_DECL_PROTO,
	SIMPLE_EXPR_PARENS,
	SIMPLE_EXPR_PRINTF,
	SIMPLE_EXPR_SIZEOF,
	SIMPLE_IMPLICIT_INT,
	SIMPLE_STATIC,
	SIMPLE_STMT,
	SIMPLE_STMT_EMPTY_LOOP,

	SIMPLE_LAST, /* sentinel */
};

struct simple_cookie {
	struct simple		*si;
	enum simple_pass	 pass;
	int			 state;
};

#define simple_cookie(varname)						\
	__attribute__((cleanup(simple_leave)))				\
	struct simple_cookie varname = {0}

struct simple	*simple_alloc(struct arena_scope *, const struct options *);

int	simple_enter(struct simple *, enum simple_pass, unsigned int,
    struct simple_cookie *);
void	simple_leave(struct simple_cookie *);
int	is_simple_enabled(const struct simple *, enum simple_pass);

int	simple_disable(struct simple *);
void	simple_enable(struct simple *, int);
