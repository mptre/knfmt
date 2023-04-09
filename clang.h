struct lexer;
struct options;
struct style;

void	clang_init(void);
void	clang_shutdown(void);

struct clang	*clang_alloc(const struct style *, const struct options *);
void		 clang_free(struct clang *);

struct token	*clang_read(struct lexer *, void *);
