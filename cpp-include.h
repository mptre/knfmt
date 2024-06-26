struct arena;
struct arena_scope;
struct lexer;
struct options;
struct simple;
struct style;
struct token;
struct token_list;

struct cpp_include	*cpp_include_alloc(const struct style *,
    struct simple *, struct token_list *, struct arena_scope *, struct arena *,
    const struct options *);
void			 cpp_include_done(struct cpp_include *);
void			 cpp_include_leave(struct cpp_include *,
    struct lexer *);
void			 cpp_include_add(struct cpp_include *, struct lexer *,
    struct token *);
