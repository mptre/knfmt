struct lexer;
struct simple;
struct style;
struct token;
struct token_list;

struct cpp_include	*cpp_include_alloc(const struct style *,
    struct simple *);
void			 cpp_include_free(struct cpp_include *);
void			 cpp_include_enter(struct cpp_include *, struct lexer *,
    struct token_list *);
void			 cpp_include_leave(struct cpp_include *);
void			 cpp_include_add(struct cpp_include *, struct token *);
