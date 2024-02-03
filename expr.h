struct expr_exec_arg {
	const struct style	*st;
	struct simple		*si;
	const struct options	*op;
	struct lexer		*lx;
	struct ruler		*rl;
	struct doc		*dc;

	/*
	 * Reaching this token causes the expression parser to stop.
	 * Passing NULL instructs the parser to continue until reaching
	 * something unknown.
	 */
	const struct token	*stop;

	unsigned int		 indent;
	/*
	 * Optional alignment taking higher predence than indent when
	 * clang-format is enabled.
	 */
	unsigned int		 align;
	unsigned int		 flags;
/* Emit a soft line before the expression. */
#define EXPR_EXEC_SOFTLINE		0x00000001u
/* Emit a hard line before the expression. */
#define EXPR_EXEC_HARDLINE		0x00000002u
/* Align arguments using the supplied ruler. */
#define EXPR_EXEC_ALIGN			0x00000004u
/* was EXPR_EXEC_NOPARENS		0x00000008u */
/* was EXPR_EXEC_ASM			0x00000010u */
/* Disable expr_doc_soft() logic. */
#define EXPR_EXEC_NOSOFT		0x00000020u

	struct {
		/*
		 * Invoked when an invalid expression is encountered.
		 * Returning anything other than NULL implies that the
		 * expression parser can continue.
		 */
		struct doc	*(*recover)(const struct expr_exec_arg *, void *);

		/*
		 * Expected to consume a type as part of a cast expression.
		 * Returning anything other than NULL implies that the
		 * expression parser can continue.
		 */
		struct doc	*(*recover_cast)(const struct expr_exec_arg *,
		    void *);

		/* Opaque argument passed to recover routines. */
		void		*arg;
	} callbacks;
};

void	expr_init(void);
void	expr_shutdown(void);

struct doc	*expr_exec(const struct expr_exec_arg *);
int		 expr_peek(const struct expr_exec_arg *, struct token **);
