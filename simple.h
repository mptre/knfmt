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
	SIMPLE_EXPR_PARENS,
	SIMPLE_EXPR_SIZEOF,
	SIMPLE_STATIC,
	SIMPLE_STMT,
	SIMPLE_STMT_SEMI,

	SIMPLE_LAST, /* sentinel */
};

struct simple_cookie {
	struct simple		*si;
	enum simple_pass	 pass;
	int			 state;
};

#define SIMPLE_COOKIE	__attribute__((cleanup(simple_leave))) \
			struct simple_cookie

struct simple	*simple_alloc(const struct options *);
void		 simple_free(struct simple *);

int	simple_enter(struct simple *, enum simple_pass, unsigned int,
    struct simple_cookie *);
void	simple_leave(struct simple_cookie *);
int	is_simple_enabled(const struct simple *, enum simple_pass);

int	simple_disable(struct simple *);
void	simple_enable(struct simple *, int);
