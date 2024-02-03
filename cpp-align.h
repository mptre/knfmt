struct arena;
struct options;
struct style;
struct token;

char	*cpp_align(struct token *, const struct style *, struct arena *,
    const struct options *);
