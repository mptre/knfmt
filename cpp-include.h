struct options;
struct style;
struct token;
struct token_list;

struct cpp_include	*cpp_include_alloc(const struct options *,
    const struct style *);
void			 cpp_include_free(struct cpp_include *);
void			 cpp_include_enter(struct cpp_include *,
    struct token_list *);
void			 cpp_include_leave(struct cpp_include *);
void			 cpp_include_add(struct cpp_include *, struct token *);
