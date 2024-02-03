struct arena;
struct lexer;
struct options;
struct simple;
struct style;

void	clang_init(void);
void	clang_shutdown(void);

struct clang	*clang_alloc(const struct style *, struct simple *,
    struct arena *, const struct options *);
void		 clang_free(struct clang *);

struct token	*clang_read(struct lexer *, void *);
struct token	*clang_token_alloc(const struct token *);
