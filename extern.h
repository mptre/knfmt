#include "config.h"

#include "cdefs.h"
#include "token-type.h"

#ifdef HAVE_QUEUE
#  include <sys/queue.h>
#else
#  include "compat-queue.h"
#endif

/*
 * options ---------------------------------------------------------------------
 */

struct options {
	unsigned int	op_flags;
#define OPTIONS_FLAG_DIFF		0x00000001u
#define OPTIONS_FLAG_DIFFPARSE		0x00000002u
#define OPTIONS_FLAG_INPLACE		0x00000004u
#define OPTIONS_FLAG_SIMPLE		0x00000008u
#define OPTIONS_FLAG_TEST		0x80000000u

	unsigned int	op_verbose;
	unsigned int	op_mw;		/* max width per line */
	unsigned int	op_tw;		/* tab width */
	unsigned int	op_sw;		/* soft width */
};

void	options_init(struct options *);

static inline int
options_trace(const struct options *op)
{
	return UNLIKELY(op->op_verbose >= 2);
}

/*
 * token -----------------------------------------------------------------------
 */

TAILQ_HEAD(token_list, token);

struct token {
	enum token_type		 tk_type;
	int			 tk_refs;
	unsigned int		 tk_lno;
	unsigned int		 tk_cno;
	unsigned int		 tk_flags;
/* Token denotes a type keyword, see token.h. */
#define TOKEN_FLAG_TYPE		0x00000001u
/* Token denotes a qualifier keyword, see token.h. */
#define TOKEN_FLAG_QUALIFIER	0x00000002u
/* Token denotes a storage keyword, see token.h. */
#define TOKEN_FLAG_STORAGE	0x00000004u
/*
 * Token optionally be followed by an identifier. Only applicable to struct,
 * enum and union.
 */
#define TOKEN_FLAG_IDENT	0x00000008u
/* Token is either a prefix or suffix. */
#define TOKEN_FLAG_DANGLING	0x00000010u
/* Token denotes an assignment operator, see token.h. */
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
int	 token_has_prefix(const struct token *, enum token_type);
int	 token_has_tabs(const struct token *);
int	 token_has_spaces(const struct token *);
int	 token_is_branch(const struct token *);
int	 token_is_decl(const struct token *, enum token_type);
int	 token_is_moveable(const struct token *);
int	 token_trim(struct token *);
char	*token_sprintf(const struct token *);

void	token_list_copy(struct token_list *, struct token_list *);
void	token_list_move(struct token_list *, struct token_list *);

/*
 * lexer -----------------------------------------------------------------------
 */

struct buffer;
struct error;
struct file;

struct lexer_state {
	struct token	*st_tok;
	unsigned int	 st_lno;
	unsigned int	 st_cno;
	unsigned int	 st_err;
	size_t		 st_off;
};

void		 lexer_init(void);
void		 lexer_shutdown(void);
struct lexer	*lexer_alloc(const struct file *, const struct buffer *,
    struct error *, const struct options *);
void		 lexer_free(struct lexer *);

int	lexer_get_error(const struct lexer *);
int	lexer_get_lines(const struct lexer *, unsigned int,
    unsigned int, const char **, size_t *);

void	lexer_stamp(struct lexer *);
int	lexer_recover(struct lexer *);
int	lexer_branch(struct lexer *);

int	lexer_is_branch(const struct lexer *);

int	lexer_pop(struct lexer *, struct token **);
int	lexer_back(const struct lexer *, struct token **);

struct token	*lexer_copy_after(struct lexer *, struct token *,
    const struct token *);
struct token	*lexer_insert_before(struct lexer *, struct token *,
    enum token_type, const char *);
struct token	*lexer_insert_after(struct lexer *, struct token *,
    enum token_type, const char *);
struct token	*lexer_move_after(struct lexer *, struct token *,
    struct token *);
struct token	*lexer_move_before(struct lexer *, struct token *,
    struct token *);
void		 lexer_remove(struct lexer *, struct token *, int);

#define lexer_expect(a, b, c) \
	__lexer_expect((a), (b), (c), __func__, __LINE__)
int	__lexer_expect(struct lexer *, enum token_type, struct token **,
    const char *, int);

void	lexer_peek_enter(struct lexer *, struct lexer_state *);
void	lexer_peek_leave(struct lexer *, struct lexer_state *);

int	lexer_peek(struct lexer *, struct token **);

#define LEXER_TYPE_FLAG_CAST	0x00000001u
#define LEXER_TYPE_FLAG_ARG	0x00000002u

int	lexer_peek_if_type(struct lexer *, struct token **, unsigned int);
int	lexer_if_type(struct lexer *, struct token **, unsigned int);

int	lexer_peek_if(struct lexer *, enum token_type, struct token **);
int	lexer_if(struct lexer *, enum token_type, struct token **);

int	lexer_peek_if_flags(struct lexer *, unsigned int, struct token **);
int	lexer_if_flags(struct lexer *, unsigned int, struct token **);

int	lexer_peek_if_pair(struct lexer *, enum token_type, enum token_type,
    struct token **);
int	lexer_if_pair(struct lexer *, enum token_type, enum token_type,
    struct token **);

int	lexer_peek_if_prefix_flags(struct lexer *, unsigned int,
    struct token **);

int	lexer_peek_until(struct lexer *, enum token_type, struct token **);
int	lexer_peek_until_loose(struct lexer *, enum token_type,
    const struct token *, struct token **);
int	lexer_until(struct lexer *, enum token_type, struct token **);

const struct diffchunk	*lexer_get_diffchunk(const struct lexer *,
    unsigned int);

void	lexer_dump(const struct lexer *);
