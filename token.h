#include "config.h"

#ifdef HAVE_QUEUE
#  include <sys/queue.h>
#else
#  include "compat-queue.h"
#endif

TAILQ_HEAD(token_list, token);

struct token {
	int			 tk_type;
	int			 tk_refs;
	unsigned int		 tk_lno;
	unsigned int		 tk_cno;
	unsigned int		 tk_flags;
/* Token denotes a type keyword, see token-defs.h. */
#define TOKEN_FLAG_TYPE		0x00000001u
/* Token denotes a qualifier keyword, see token-defs.h. */
#define TOKEN_FLAG_QUALIFIER	0x00000002u
/* Token denotes a storage keyword, see token-defs.h. */
#define TOKEN_FLAG_STORAGE	0x00000004u
/*
 * Token optionally be followed by an identifier. Only applicable to struct,
 * enum and union.
 */
#define TOKEN_FLAG_IDENT	0x00000008u
/* Token is either a prefix or suffix. */
#define TOKEN_FLAG_DANGLING	0x00000010u
/* Token denotes an assignment operator, see token-defs.h. */
#define TOKEN_FLAG_ASSIGN	0x00000020u
#define TOKEN_FLAG_AMBIGUOUS	0x00000040u
#define TOKEN_FLAG_BINARY	0x00000080u
#define TOKEN_FLAG_DISCARD	0x00000100u
#define TOKEN_FLAG_UNMUTE	0x00000200u
#define TOKEN_FLAG_DIRTY	0x00000400u
#define TOKEN_FLAG_CPP		0x00000800u
/* was  TOKEN_FLAG_FREE		0x00001000u */
/*
 * Token followed by exactly one new line. Dangling suffix and only emitted
 * in certain contexts.
 */
#define TOKEN_FLAG_OPTLINE	0x00002000u
/* Token followed by spaces or tabs. Dangling suffix and never emitted. */
#define TOKEN_FLAG_OPTSPACE	0x00004000u
/*
 * Token may be surrounded by spaces. Only applicable to binary operators, see
 * token.h.
 */
#define TOKEN_FLAG_SPACE	0x00008000u
/* Token covered by diff chunk. */
#define TOKEN_FLAG_DIFF		0x00010000u
/*
 * Token denotes the start of arguments to a function pointer type.
 * Only applicable to TOKEN_LPAREN.
 */
#define TOKEN_FLAG_TYPE_ARGS	0x08000000u
#define TOKEN_FLAG_TYPE_FUNC	0x10000000u

	size_t			 tk_off;

	const char		*tk_str;
	size_t			 tk_len;

	union {
		struct token	*tk_token;
	};

	struct {
		struct token	*br_pv;
		struct token	*br_nx;
	} tk_branch;

	struct token_list	 tk_prefixes;
	struct token_list	 tk_suffixes;

	TAILQ_ENTRY(token)	 tk_entry;
	TAILQ_ENTRY(token)	 tk_stamp;
};

void	 token_ref(struct token *);
void	 token_rele(struct token *);
void	 token_add_optline(struct token *);
int	 token_cmp(const struct token *, const struct token *);
int	 token_has_indent(const struct token *);
int	 token_has_line(const struct token *, int);
int	 token_has_prefix(const struct token *, int);
int	 token_has_tabs(const struct token *);
int	 token_has_spaces(const struct token *);
int	 token_is_branch(const struct token *);
int	 token_is_decl(const struct token *, int);
int	 token_is_moveable(const struct token *);
int	 token_trim(struct token *);
char	*token_sprintf(const struct token *);

#define token_next(tk) __token_next((struct token *)(tk))
struct token	*__token_next(struct token *);

#define token_prev(tk) __token_prev((struct token *)(tk))
struct token	*__token_prev(struct token *);

void	token_list_copy(struct token_list *, struct token_list *);
void	token_list_move(struct token_list *, struct token_list *);

void	token_move_suffixes(struct token *, struct token *, int);
