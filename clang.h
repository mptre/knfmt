struct arena;
struct arena_scope;
struct buffer;
struct lexer;
struct options;
struct simple;
struct style;

void	clang_init(void);
void	clang_shutdown(void);

struct clang	*clang_alloc(const struct style *, struct simple *,
    struct arena_scope *, struct arena *, const struct options *);
void		 clang_free(struct clang *);

struct lexer_callbacks	clang_lexer_callbacks(struct clang *);
