#include "config.h"

#ifdef HAVE_QUEUE
#  include <sys/queue.h>
#else
#  include "compat-queue.h"
#endif

/* Annotate the argument as unused. */
#define UNUSED(x)	_##x __attribute__((__unused__))
#define MAYBE_UNUSED(x)	x __attribute__((__unused__))

#define UNLIKELY(x)	__builtin_expect((x), 0)

#define TRACE(cf)	(UNLIKELY((cf)->cf_verbose >= 2))

/*
 * config ----------------------------------------------------------------------
 */

struct config {
	unsigned int	cf_flags;
#define CONFIG_FLAG_DIFF		0x00000001u
#define CONFIG_FLAG_DIFFPARSE		0x00000002u
#define CONFIG_FLAG_INPLACE		0x00000004u
#define CONFIG_FLAG_SIMPLE		0x00000008u
#define CONFIG_FLAG_TEST		0x80000000u

	unsigned int	cf_verbose;
	unsigned int	cf_mw;		/* max width per line */
	unsigned int	cf_tw;		/* tab width */
	unsigned int	cf_sw;		/* soft width */
};

void	config_init(struct config *);

/*
 * buffer ----------------------------------------------------------------------
 */

struct buffer {
	char	*bf_ptr;
	size_t	 bf_siz;
	size_t	 bf_len;
};

struct buffer	*buffer_alloc(size_t);
struct buffer	*buffer_read(const char *);
void		 buffer_free(struct buffer *);
void		 buffer_append(struct buffer *, const char *, size_t);
void		 buffer_appendc(struct buffer *, char);
void		 buffer_appendv(struct buffer *, const char *, ...)
	__attribute__((__format__(printf, 2, 3)));
char		*buffer_release(struct buffer *);
void		 buffer_reset(struct buffer *);
int		 buffer_cmp(const struct buffer *, const struct buffer *);

/*
 * error -----------------------------------------------------------------------
 */

struct error {
	const struct config	*er_cf;
	struct buffer		*er_bf;
};

void		 error_init(struct error *, const struct config *);
void		 error_close(struct error *);
void		 error_reset(struct error *);
void		 error_flush(struct error *);
struct buffer	*error_get_buffer(struct error *);

#define error_write(er, fmt, ...) do {					\
	buffer_appendv(error_get_buffer((er)), (fmt), __VA_ARGS__);	\
	if (TRACE((er)->er_cf))						\
		error_flush((er));					\
} while (0)

/*
 * diff ------------------------------------------------------------------------
 */

struct file_list;

struct diffchunk {
	unsigned int		du_beg;
	unsigned int		du_end;

	TAILQ_ENTRY(diffchunk)	du_entry;
};

TAILQ_HEAD(diffchunk_list, diffchunk);

struct diff {
	struct diffchunk_list	di_chunks;
};

void	diff_init(void);
void	diff_shutdown(void);
int	diff_parse(struct file_list *, const struct config *);
int	diff_covers(const struct diff *, unsigned int);

/*
 * file ------------------------------------------------------------------------
 */

struct file {
	struct diff		 fe_diff;
	char			*fe_path;
	unsigned int		 fe_flags;
#define FILE_FLAG_FREE		0x00000001u	/* fe_path must be freed */

	TAILQ_ENTRY(file)	 fe_entry;
};

TAILQ_HEAD(file_list, file);

void	files_free(struct file_list *);

struct file	*file_alloc(char *, unsigned int);
void		 file_free(struct file *);

/*
 * token -----------------------------------------------------------------------
 */

enum token_type {
#define T(t, s, f) t,
#include "token.h"
};

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

struct lexer_state {
	struct token	*st_tok;
	unsigned int	 st_lno;
	unsigned int	 st_cno;
	unsigned int	 st_err;
	size_t		 st_off;
};

void		 lexer_init(void);
void		 lexer_shutdown(void);
struct lexer	*lexer_alloc(const struct file *, struct error *,
    const struct config *);
void		 lexer_free(struct lexer *);

const struct buffer	*lexer_get_buffer(const struct lexer *);
int			 lexer_get_error(const struct lexer *);
int			 lexer_get_lines(const struct lexer *, unsigned int,
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
int	lexer_peek_until_stop(struct lexer *, enum token_type,
    const struct token *, struct token **);
#define lexer_until(a, b, c) \
	__lexer_until((a), (b), NULL, (c), __func__, __LINE__)
int	__lexer_until(struct lexer *, enum token_type, const struct token *,
    struct token **, const char *, int);

const struct diffchunk	*lexer_get_diffchunk(const struct lexer *,
    unsigned int);

void	lexer_dump(const struct lexer *);

/*
 * parser ----------------------------------------------------------------------
 */

struct parser		*parser_alloc(const char *, struct lexer *,
    struct error *, const struct config *);
void			 parser_free(struct parser *);
const struct buffer	*parser_exec(struct parser *);
struct doc		*parser_exec_expr_recover(unsigned int, void *);

/*
 * simple ----------------------------------------------------------------------
 */

struct simple_decl	*simple_decl_enter(struct lexer *,
    const struct config *);
void			 simple_decl_leave(struct simple_decl *);
void			 simple_decl_free(struct simple_decl *);
void			 simple_decl_type(struct simple_decl *, struct token *,
    struct token *);
void			 simple_decl_semi(struct simple_decl *, struct token *);
void			 simple_decl_comma(struct simple_decl *,
    struct token *);

struct simple_stmt	*simple_stmt_enter(struct lexer *,
    const struct config *);
void			 simple_stmt_leave(struct simple_stmt *);
void			 simple_stmt_free(struct simple_stmt *);
struct doc		*simple_stmt_block(struct simple_stmt *,
    struct token *, struct token *, int);
void			*simple_stmt_ifelse_enter(struct simple_stmt *,
    struct token *, int);
void			 simple_stmt_ifelse_leave(struct simple_stmt *,
    struct token *, void *);

/*
 * expr ------------------------------------------------------------------------
 */

struct expr_exec_arg {
	const struct config	*ea_cf;
	struct lexer		*ea_lx;
	struct doc		*ea_dc;
	const struct token	*ea_stop;

	/*
	 * Callback invoked when an invalid expression is encountered. If the
	 * same callback returns a document implies that the expression parser
	 * can continue.
	 */
	struct doc		*(*ea_recover)(unsigned int, void *);
	void			*ea_arg;
#define EXPR_RECOVER_FLAG_CAST	0x00000001u

	unsigned int		 ea_flags;
#define EXPR_EXEC_FLAG_SOFTLINE		0x00000001u
#define EXPR_EXEC_FLAG_HARDLINE		0x00000002u
#define EXPR_EXEC_FLAG_NOINDENT		0x00000004u
#define EXPR_EXEC_FLAG_NOPARENS		0x00000008u
};

struct doc	*expr_exec(const struct expr_exec_arg *);
int		 expr_peek(const struct expr_exec_arg *);

/*
 * cpp -------------------------------------------------------------------------
 */

char	*cpp_exec(const struct token *, const struct config *);

/*
 * doc -------------------------------------------------------------------------
 */

/* Keep in sync with DESIGN. */
enum doc_type {
	DOC_CONCAT,
	DOC_GROUP,
	DOC_INDENT,
	DOC_DEDENT,
	DOC_ALIGN,
	DOC_LITERAL,
	DOC_VERBATIM,
	DOC_LINE,
	DOC_SOFTLINE,
	DOC_HARDLINE,
	DOC_OPTLINE,
	DOC_MUTE,
	DOC_OPTIONAL,
};

#define DOC_EXEC_FLAG_NODIFF	0x00000001u
#define DOC_EXEC_FLAG_NOTRACE	0x00000002u

void		doc_exec(const struct doc *, struct lexer *, struct buffer *,
    const struct config *, unsigned int);
unsigned int	doc_width(const struct doc *, struct buffer *,
    const struct config *);
void		doc_free(struct doc *);
void		doc_append(struct doc *, struct doc *);
void		doc_append_before(struct doc *, struct doc *);
void		doc_remove(struct doc *, struct doc *);
void		doc_remove_tail(struct doc *);
void		doc_set_indent(struct doc *, int);

#define doc_alloc(a, b) \
	__doc_alloc((a), (b), 0, __func__, __LINE__)
struct doc	*__doc_alloc(enum doc_type, struct doc *, int, const char *,
    int);

/*
 * Sentinels honored by document allocation routines. The numbers are something
 * arbitrary large enough to never conflict with any actual value. Since the
 * numbers are compared with signed integers, favor integer literals over
 * hexadecimal ones.
 */
#define DOC_INDENT_PARENS	111	/* entering parenthesis */
#define DOC_INDENT_FORCE	222	/* force indentation */

#define doc_alloc_indent(a, b) \
	__doc_alloc_indent(DOC_INDENT, (a), (b), __func__, __LINE__)
#define doc_alloc_dedent(a) \
	__doc_alloc_indent(DOC_DEDENT, 0, (a), __func__, __LINE__)
struct doc	*__doc_alloc_indent(enum doc_type, int, struct doc *,
    const char *, int);

#define doc_literal(a, b) \
	__doc_literal((a), strlen(a), (b), __func__, __LINE__)
#define doc_literal_n(a, b, c) \
	__doc_literal((a), (b), (c), __func__, __LINE__)
struct doc	*__doc_literal(const char *, size_t, struct doc *,
    const char *, int);

#define doc_token(a, b) \
	__doc_token((a), (b), DOC_LITERAL, __func__, __LINE__)
struct doc	*__doc_token(const struct token *, struct doc *, enum doc_type,
    const char *, int);

int	doc_max(const struct doc *);

void	doc_annotate(struct doc *, const char *);

/*
 * ruler -----------------------------------------------------------------------
 */

struct ruler {
	struct {
		struct ruler_column	*b_ptr;
		size_t			 b_len;
		size_t			 b_siz;
	} rl_columns;

	struct ruler_indent_list	*rl_indent;
	size_t				 rl_len;
};

struct ruler_column {
	struct {
		struct ruler_datum	*b_ptr;
		size_t			 b_len;
		size_t			 b_siz;
	} rc_datums;

	size_t	rc_len;
	size_t	rc_nspaces;
	size_t	rc_ntabs;
};

void	ruler_init(struct ruler *, unsigned int);
void	ruler_free(struct ruler *);
void	ruler_exec(struct ruler *);

#define ruler_insert(a, b, c, d, e, f) \
	__ruler_insert((a), (b), (c), (d), (e), (f), __func__, __LINE__)
void	__ruler_insert(struct ruler *, const struct token *, struct doc *,
    unsigned int, unsigned int, unsigned int, const char *, int);

#define ruler_indent(a, b) \
	__ruler_indent((a), (b), 1, __func__, __LINE__)
#define ruler_dedent(a, b) \
	__ruler_indent((a), (b), -1, __func__, __LINE__)
struct doc	*__ruler_indent(struct ruler *, struct doc *, int,
    const char *, int);

/*
 * util ------------------------------------------------------------------------
 */

char	*strnice(const char *, size_t);
