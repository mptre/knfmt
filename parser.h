#include <stddef.h>	/* size_t */

struct lexer;
struct options;
struct simple;
struct style;

struct parser	*parser_alloc(const char *, struct lexer *,
    const struct style *, struct simple *, const struct options *);
void		 parser_free(struct parser *);
struct buffer	*parser_exec(struct parser *, size_t);
