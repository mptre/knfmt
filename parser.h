#include <stddef.h>	/* size_t */

struct error;
struct lexer;
struct options;

struct parser	*parser_alloc(const char *, struct lexer *, struct error *,
    const struct options *);
void		 parser_free(struct parser *);
struct buffer	*parser_exec(struct parser *, size_t);
struct doc	*parser_exec_expr_recover(unsigned int, void *);
