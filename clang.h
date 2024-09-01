#include "lexer-callbacks.h"

struct arena;
struct arena_scope;
struct lexer;
struct options;
struct simple;
struct style;
struct token;

/* Identifiers with support for constant time lookup. */
#define FOR_CLANG_IDENTIFIERS(OP)					\
	/* simple-expr-printf.c */					\
	OP(CLANG_TOKEN_ERR,		"err")				\
	OP(CLANG_TOKEN_ERRC,		"errc")				\
	OP(CLANG_TOKEN_ERRX,		"errx")				\
	OP(CLANG_TOKEN_PERROR,		"perror")			\
	OP(CLANG_TOKEN_VERR,		"verr")				\
	OP(CLANG_TOKEN_VERRC,		"verrc")			\
	OP(CLANG_TOKEN_VERRX,		"verrx")			\
	OP(CLANG_TOKEN_VWARN,		"vwarn")			\
	OP(CLANG_TOKEN_VWARNC,		"vwarnc")			\
	OP(CLANG_TOKEN_VWARNX,		"vwarnx")			\
	OP(CLANG_TOKEN_WARN,		"warn")				\
	OP(CLANG_TOKEN_WARNC,		"warnc")			\
	OP(CLANG_TOKEN_WARNX,		"warnx")			\
	/* parser-cpp.c */						\
	OP(CLANG_TOKEN_LIST_ENTRY,	"LIST_ENTRY")

enum clang_token_type {
	CLANG_TOKEN_NONE = 0,

#define OP(type, ...) type,
	FOR_CLANG_IDENTIFIERS(OP)
#undef OP
};

void	clang_init(void);
void	clang_shutdown(void);

struct clang	*clang_alloc(const struct style *, struct simple *,
    struct arena_scope *, struct arena *, const struct options *);

struct lexer_callbacks	clang_lexer_callbacks(struct clang *);

void	clang_stamp(struct clang *, struct lexer *);
int	clang_branch(struct clang *, struct lexer *, struct token **);
int	clang_recover(struct clang *, struct lexer *, struct token **);

void	clang_token_move_prefixes(struct token *, struct token *);

struct token	*clang_token_branch_next(struct token *);
struct token	*clang_token_branch_parent(struct token *);
void		 clang_token_branch_unlink(struct token *);

const struct token	*clang_keyword_token(int);
enum clang_token_type	 clang_token_type(const struct token *);
