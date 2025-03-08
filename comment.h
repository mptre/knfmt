struct arena;
struct arena_scope;
struct style;
struct token;

const char	*comment_trim(const struct token *, const struct style *,
    struct arena *, struct arena_scope *);
