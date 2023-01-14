#include <stddef.h>	/* size_t */

struct doc;
struct error;
struct expr_exec_arg;
struct lexer;
struct options;
struct style;

struct parser	*parser_alloc(const char *, struct lexer *, struct error *,
    const struct style *, const struct options *);
void		 parser_free(struct parser *);
struct buffer	*parser_exec(struct parser *, size_t);

int	parser_expr_recover(const struct expr_exec_arg *, struct doc *, void *);
