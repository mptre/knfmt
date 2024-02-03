struct arena;
struct arena_scope;
struct buffer;
struct diffchunk;
struct lexer;
struct options;
struct simple;
struct style;

struct parser	*parser_alloc(struct lexer *, const struct style *,
    struct simple *, struct arena_scope *, struct arena *,
    const struct options *);
int		 parser_exec(struct parser *, const struct diffchunk *,
    struct buffer *);
