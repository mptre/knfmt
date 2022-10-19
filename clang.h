struct lexer;
struct options;

struct clang	*clang_alloc(const struct options *);
void		 clang_free(struct clang *);

struct token	*clang_read(struct lexer *, void *);
