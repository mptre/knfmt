#include "lexer-callbacks.h"

struct arena;
struct arena_scope;
struct lexer;
struct options;
struct simple;
struct style;
struct token;

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
