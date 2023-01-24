struct lexer;
struct options;
struct style;

void	clang_init(void);
void	clang_shutdown(void);

struct clang	*clang_alloc(const struct options *, const struct style *);
void		 clang_free(struct clang *);

struct token	*clang_read(struct lexer *, void *);
