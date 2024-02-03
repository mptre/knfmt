struct arena;
struct arena_scope;
struct options;
struct style;
struct token;

struct buffer	*cpp_align(struct token *, const struct style *,
    struct arena_scope *, struct arena *, const struct options *);
