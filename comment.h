struct arena_scope;
struct style;
struct token;

struct buffer	*comment_trim(const struct token *, const struct style *,
    struct arena_scope *);
