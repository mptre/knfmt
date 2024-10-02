struct arena;
struct arena_scope;
struct options;
struct style;
struct token;

const char	*cpp_align(struct token *, const struct style *,
    struct arena_scope *, struct arena *, struct arena *,
    const struct options *);
