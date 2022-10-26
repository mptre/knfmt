struct expr_exec_arg {
	const struct style	*st;
	const struct options	*op;
	struct lexer		*lx;
	struct doc		*dc;
	const struct token	*stop;

	unsigned int		 indent;
	unsigned int		 flags;
/* Emit a soft line before the expression. */
#define EXPR_EXEC_SOFTLINE		0x00000001u
/* Emit a hard line before the expression. */
#define EXPR_EXEC_HARDLINE		0x00000002u
/*
 * Only account for the indent field once. Necessary when the dc field does not
 * cover the complete line.
 */
/* was EXPR_EXEC_INDENT_ONCE		0x00000004u */
/* Trim redundant parenthesis around top level expression. */
#define EXPR_EXEC_NOPARENS		0x00000008u
/* Detect inline assembly operands. */
#define EXPR_EXEC_ASM			0x00000010u
/* During recovery, signal than a function argument could be present. */
#define EXPR_EXEC_ARG			0x00000020u
/* During recovery, signal than a cast could be present. */
#define EXPR_EXEC_CAST			0x00000040u

	struct {
		/*
		 * Invoked when an invalid expression is encountered. Returning
		 * zero implies that the expression parser can continue.
		 */
		int	 (*recover)(const struct expr_exec_arg *, struct doc *,
		    void *);

		void	*arg;
	} callbacks;
};

struct doc	*expr_exec(const struct expr_exec_arg *);
int		 expr_peek(const struct expr_exec_arg *);
